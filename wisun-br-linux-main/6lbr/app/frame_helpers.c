/*
 * SPDX-License-Identifier: LicenseRef-MSLA
 * Copyright (c) 2023 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include "common/log.h"
#include "common/iobuf.h"
#include "common/ieee802154_ie.h"
#include "common/string_extra.h"
#include "common/memutils.h"
#include "common/endian.h"
#include "common/bits.h"
#include "common/specs/ieee802154.h"

#include "net/protocol.h"

#include "rcp_api_legacy.h"
#include "wsbr_mac.h"
#include "frame_helpers.h"

// IEEE 802.15.4-2020 Figure 7-2 Format of the Frame Control field
#define IEEE802154_FCF_FRAME_TYPE         0b0000000000000111
#define IEEE802154_FCF_SECURITY_ENABLED   0b0000000000001000
#define IEEE802154_FCF_FRAME_PENDING      0b0000000000010000
#define IEEE802154_FCF_ACK_REQ            0b0000000000100000
#define IEEE802154_FCF_PAN_ID_COMPRESSION 0b0000000001000000
#define IEEE802154_FCF_SEQ_NUM_SUPPR      0b0000000100000000
#define IEEE802154_FCF_IE_PRESENT         0b0000001000000000
#define IEEE802154_FCF_DST_ADDR_MODE      0b0000110000000000
#define IEEE802154_FCF_FRAME_VERSION      0b0011000000000000
#define IEEE802154_FCF_SRC_ADDR_MODE      0b1100000000000000

#define IEEE802154_SECURITY_LEVEL             0b00000111
#define IEEE802154_SECURITY_KEY_MODE          0b00011000
#define IEEE802154_SECURITY_FRAME_COUNT_SUPPR 0b00100000
#define IEEE802154_SECURITY_ASN_IN_NONCE      0b01000000

// IEEE 802.15.4-2020 Figure 7-21 Format of Header IEs
#define IEEE802154_HEADER_IE_LEN_MASK  0b0000000001111111
#define IEEE802154_HEADER_IE_ID_MASK   0b0111111110000000
#define IEEE802154_HEADER_IE_TYPE_MASK 0b1000000000000000
#define IEEE802154_HEADER_IE(len, id) (               \
      FIELD_PREP(IEEE802154_HEADER_IE_LEN_MASK,  len) \
    | FIELD_PREP(IEEE802154_HEADER_IE_ID_MASK,   id ) \
    | FIELD_PREP(IEEE802154_HEADER_IE_TYPE_MASK, 0  ) \
)

// IEEE 802.15.4-2020 Figure 7-47 Format of Payload IEs
#define IEEE802154_PAYLOAD_IE_LEN_MASK  0b0000011111111111
#define IEEE802154_PAYLOAD_IE_ID_MASK   0b0111100000000000
#define IEEE802154_PAYLOAD_IE_TYPE_MASK 0b1000000000000000
#define IEEE802154_PAYLOAD_IE(len, id) (               \
      FIELD_PREP(IEEE802154_PAYLOAD_IE_LEN_MASK,  len) \
    | FIELD_PREP(IEEE802154_PAYLOAD_IE_ID_MASK,   id ) \
    | FIELD_PREP(IEEE802154_PAYLOAD_IE_TYPE_MASK, 1  ) \
)

// IEEE 802.15.4-2020 Table 7-7 Element IDs for Header IEs
#define IEEE802154_IE_ID_HT1 0x7e
#define IEEE802154_IE_ID_HT2 0x7f
// IEEE 802.15.4-2020 Table 7-17 Payload IE Group ID
#define IEEE802154_IE_ID_PT  0xf

// IEEE 802.15.4-2020 7.4.2.18 Header Termination 1 IE
#define IEEE802154_IE_HT1 IEEE802154_HEADER_IE(0, IEEE802154_IE_ID_HT1)
// IEEE 802.15.4-2020 7.4.2.19 Header Termination 2 IE
#define IEEE802154_IE_HT2 IEEE802154_HEADER_IE(0, IEEE802154_IE_ID_HT2)
// IEEE 802.15.4-2020 7.4.3.4 Payload Termination IE
#define IEEE802154_IE_PT IEEE802154_PAYLOAD_IE(0, IEEE802154_IE_ID_PT)

// IEEE 802.15.4-2020 Table 7-2 PAN ID Compression field value for frame version
// 0b10
static const struct {
    uint8_t dst_addr_mode;
    uint8_t src_addr_mode;
    bool dst_pan_id;
    bool src_pan_id;
    bool pan_id_compression;
} ieee802154_table_pan_id_comp[] = {
    { MAC_ADDR_MODE_NONE,   MAC_ADDR_MODE_NONE,   false, false, 0 },
    { MAC_ADDR_MODE_NONE,   MAC_ADDR_MODE_NONE,   true,  false, 1 },
    { MAC_ADDR_MODE_16_BIT, MAC_ADDR_MODE_NONE,   true,  false, 0 },
    { MAC_ADDR_MODE_64_BIT, MAC_ADDR_MODE_NONE,   true,  false, 0 },
    { MAC_ADDR_MODE_16_BIT, MAC_ADDR_MODE_NONE,   false, false, 1 },
    { MAC_ADDR_MODE_64_BIT, MAC_ADDR_MODE_NONE,   false, false, 1 },
    { MAC_ADDR_MODE_NONE,   MAC_ADDR_MODE_16_BIT, false, true,  0 },
    { MAC_ADDR_MODE_NONE,   MAC_ADDR_MODE_64_BIT, false, true,  0 },
    { MAC_ADDR_MODE_NONE,   MAC_ADDR_MODE_16_BIT, false, false, 1 },
    { MAC_ADDR_MODE_NONE,   MAC_ADDR_MODE_64_BIT, false, false, 1 },
    { MAC_ADDR_MODE_64_BIT, MAC_ADDR_MODE_64_BIT, true,  false, 0 },
    { MAC_ADDR_MODE_64_BIT, MAC_ADDR_MODE_64_BIT, false, false, 1 },
    { MAC_ADDR_MODE_16_BIT, MAC_ADDR_MODE_16_BIT, true,  true,  0 },
    { MAC_ADDR_MODE_16_BIT, MAC_ADDR_MODE_64_BIT, true,  true,  0 },
    { MAC_ADDR_MODE_64_BIT, MAC_ADDR_MODE_16_BIT, true,  true,  0 },
    { MAC_ADDR_MODE_16_BIT, MAC_ADDR_MODE_64_BIT, true,  false, 1 },
    { MAC_ADDR_MODE_64_BIT, MAC_ADDR_MODE_16_BIT, true,  false, 1 },
    { MAC_ADDR_MODE_16_BIT, MAC_ADDR_MODE_16_BIT, true,  false, 1 },
};

static int wsbr_data_sec_parse(struct iobuf_read *iobuf, struct mlme_security *sec)
{
    uint8_t scf;

    scf = iobuf_pop_u8(iobuf);
    sec->SecurityLevel = FIELD_GET(IEEE802154_SECURITY_LEVEL, scf);
    if (sec->SecurityLevel != SEC_ENC_MIC64) {
        TRACE(TR_DROP, "drop %-9s: unsupported security level", "15.4");
        return -ENOTSUP;
    }
    if (FIELD_GET(IEEE802154_SECURITY_KEY_MODE, scf) != MAC_KEY_ID_MODE_IDX) {
        TRACE(TR_DROP, "drop %-9s: unsupported security level", "15.4");
        return -ENOTSUP;
    }
    if (FIELD_GET(IEEE802154_SECURITY_FRAME_COUNT_SUPPR, scf)) {
        TRACE(TR_DROP, "drop %-9s: unsupported frame counter suppression", "15.4");
        return -ENOTSUP;
    }
    if (FIELD_GET(IEEE802154_SECURITY_ASN_IN_NONCE, scf))
        TRACE(TR_IGNORE, "ignore %-9s: ASN in nonce", "15.4");

    sec->frame_counter = iobuf_pop_le32(iobuf);
    sec->KeyIndex      = iobuf_pop_u8(iobuf);

    if (iobuf_remaining_size(iobuf) < 8) {
        TRACE(TR_DROP, "drop %-9s: missing MIC-64", "15.4");
        return -EINVAL;
    }
    iobuf->data_size -= 8;

    return 0;
}

static int wsbr_data_ie_parse(struct iobuf_read *iobuf, struct mcps_data_rx_ie_list *ie)
{
    struct iobuf_read iobuf_ie;
    int ret_ht1, ret_pt;

    memset(ie, 0, sizeof(*ie));
    ret_ht1 = ieee802154_ie_find_header(iobuf_ptr(iobuf), iobuf_remaining_size(iobuf),
                                        IEEE802154_IE_ID_HT1, &iobuf_ie);
    if (ret_ht1 < 0 && ret_ht1 != -ENOENT) {
        TRACE(TR_DROP, "drop %-9s: malformed IEs", "15.4");
        return ret_ht1;
    }
    if (!ret_ht1 || !ieee802154_ie_find_header(iobuf_ptr(iobuf), iobuf_remaining_size(iobuf),
                                                IEEE802154_IE_ID_HT2, &iobuf_ie)) {
        ie->headerIeListLength = iobuf_ptr(&iobuf_ie) - 2 - iobuf_ptr(iobuf);
        ie->headerIeList       = iobuf_pop_data_ptr(iobuf, ie->headerIeListLength);
        iobuf_pop_le16(iobuf); // Header Termination IE
    } else {
        ie->headerIeListLength = iobuf_remaining_size(iobuf);
        ie->headerIeList       = iobuf_pop_data_ptr(iobuf, ie->headerIeListLength);
    }
    if (!ret_ht1) {
        ret_pt = ieee802154_ie_find_payload(iobuf_ptr(iobuf), iobuf_remaining_size(iobuf),
                                            IEEE802154_IE_ID_PT, &iobuf_ie);
        if (ret_pt < 0 && ret_pt != -ENOENT) {
            TRACE(TR_DROP, "drop %-9s: malformed IEs", "15.4");
            return ret_pt;
        }
        if (!ret_pt) {
            ie->payloadIeListLength = iobuf_ptr(&iobuf_ie) - 2 - iobuf_ptr(iobuf);
            ie->payloadIeList       = iobuf_pop_data_ptr(iobuf, ie->headerIeListLength);
            iobuf_pop_le16(iobuf); // Payload Termination IE
        } else {
            ie->payloadIeListLength = iobuf_remaining_size(iobuf);
            ie->payloadIeList       = iobuf_pop_data_ptr(iobuf, ie->payloadIeListLength);
        }
    }
    return 0;
}

int wsbr_data_ind_parse(const uint8_t *frame, size_t frame_len,
                        struct mcps_data_ind *ind,
                        struct mcps_data_rx_ie_list *ie, uint16_t pan_id)
{
    struct iobuf_read iobuf = {
        .data_size = frame_len,
        .data = frame,
    };
    uint16_t fcf;
    int ret, i;

    fcf = iobuf_pop_le16(&iobuf);
    if (FIELD_GET(IEEE802154_FCF_FRAME_TYPE, fcf) != IEEE802154_FRAME_TYPE_DATA) {
        TRACE(TR_DROP, "drop %-9s: unsupported frame type", "15.4");
        return -ENOTSUP;
    }
    if (FIELD_GET(IEEE802154_FCF_FRAME_VERSION, fcf) != MAC_FRAME_VERSION_2015) {
        TRACE(TR_DROP, "drop %-9s: unsupported frame version", "15.4");
        return -ENOTSUP;
    }
    ind->PendingBit      = FIELD_GET(IEEE802154_FCF_FRAME_PENDING,      fcf);
    ind->TxAckReq        = FIELD_GET(IEEE802154_FCF_ACK_REQ,            fcf);
    ind->PanIdSuppressed = FIELD_GET(IEEE802154_FCF_PAN_ID_COMPRESSION, fcf);
    ind->DSN_suppressed  = FIELD_GET(IEEE802154_FCF_SEQ_NUM_SUPPR,      fcf);
    ind->DstAddrMode     = FIELD_GET(IEEE802154_FCF_DST_ADDR_MODE,      fcf);
    ind->SrcAddrMode     = FIELD_GET(IEEE802154_FCF_SRC_ADDR_MODE,      fcf);

    if (!ind->DSN_suppressed)
        ind->DSN = iobuf_pop_u8(&iobuf);

    for (i = 0; i < ARRAY_SIZE(ieee802154_table_pan_id_comp); i++)
        if (ieee802154_table_pan_id_comp[i].dst_addr_mode      == ind->DstAddrMode &&
            ieee802154_table_pan_id_comp[i].src_addr_mode      == ind->SrcAddrMode &&
            ieee802154_table_pan_id_comp[i].pan_id_compression == ind->PanIdSuppressed)
            break;
    if (i == ARRAY_SIZE(ieee802154_table_pan_id_comp)) {
        TRACE(TR_DROP, "drop %-9s: unsupported address mode", "15.4");
        return -ENOTSUP;
    }

    if (ieee802154_table_pan_id_comp[i].dst_pan_id)
        ind->DstPANId = iobuf_pop_le16(&iobuf);
    else
        ind->DstPANId = pan_id;

    if (ind->DstAddrMode == MAC_ADDR_MODE_64_BIT) {
        write_be64(ind->DstAddr, iobuf_pop_le64(&iobuf));
    } else if (ind->DstAddrMode != MAC_ADDR_MODE_NONE) {
        TRACE(TR_DROP, "drop %-9s: unsupported address mode", "15.4");
        return -ENOTSUP;
    }

    if (ieee802154_table_pan_id_comp[i].src_pan_id)
        ind->SrcPANId = iobuf_pop_le16(&iobuf);
    else
        ind->SrcPANId = ind->DstPANId;

    if (ind->SrcAddrMode == MAC_ADDR_MODE_64_BIT) {
        write_be64(ind->SrcAddr, iobuf_pop_le64(&iobuf));
    } else if (ind->SrcAddrMode != MAC_ADDR_MODE_NONE) {
        TRACE(TR_DROP, "drop %-9s: unsupported address mode", "15.4");
        return -ENOTSUP;
    }

    if (FIELD_GET(IEEE802154_FCF_SECURITY_ENABLED, fcf)) {
        ret = wsbr_data_sec_parse(&iobuf, &ind->Key);
        if (ret < 0)
            return ret;
    } else {
        memset(&ind->Key, 0, sizeof(ind->Key));
    }

    if (FIELD_GET(IEEE802154_FCF_IE_PRESENT, fcf)) {
        ret = wsbr_data_ie_parse(&iobuf, ie);
        if (ret < 0)
            return ret;
    }

    if (iobuf_remaining_size(&iobuf))
        TRACE(TR_IGNORE, "ignore %-9s: unsupported frame payload", "15.4");

    if (iobuf.err) {
        TRACE(TR_DROP, "drop %-9s: malformed packet", "15.4");
        return -EINVAL;
    }

    return 0;
}

int wsbr_data_cnf_parse(const uint8_t *frame, size_t frame_len,
                        struct mcps_data_cnf *cnf,
                        struct mcps_data_rx_ie_list *ie)
{
    struct iobuf_read iobuf = {
        .data_size = frame_len,
        .data = frame,
    };
    uint8_t src_addr_mode, dst_addr_mode;
    bool pan_id_cmpr;
    uint16_t fcf;
    int ret, i;

    memset(ie, 0, sizeof(*ie));

    fcf = iobuf_pop_le16(&iobuf);
    switch (FIELD_GET(IEEE802154_FCF_FRAME_TYPE, fcf)) {
    case IEEE802154_FRAME_TYPE_DATA:
    case IEEE802154_FRAME_TYPE_ACK:
        break;
    default:
        TRACE(TR_DROP, "drop %-9s: unsupported frame type", "15.4");
        return -ENOTSUP;
    }
    if (FIELD_GET(IEEE802154_FCF_FRAME_VERSION, fcf) != MAC_FRAME_VERSION_2015) {
        TRACE(TR_DROP, "drop %-9s: unsupported frame version", "15.4");
        return -ENOTSUP;
    }
    pan_id_cmpr   = FIELD_GET(IEEE802154_FCF_PAN_ID_COMPRESSION, fcf);
    dst_addr_mode = FIELD_GET(IEEE802154_FCF_DST_ADDR_MODE,      fcf);
    src_addr_mode = FIELD_GET(IEEE802154_FCF_SRC_ADDR_MODE,      fcf);

    if (!FIELD_GET(IEEE802154_FCF_SEQ_NUM_SUPPR, fcf))
        iobuf_pop_u8(&iobuf);

    for (i = 0; i < ARRAY_SIZE(ieee802154_table_pan_id_comp); i++)
        if (ieee802154_table_pan_id_comp[i].dst_addr_mode      == dst_addr_mode &&
            ieee802154_table_pan_id_comp[i].src_addr_mode      == src_addr_mode &&
            ieee802154_table_pan_id_comp[i].pan_id_compression == pan_id_cmpr)
            break;
    if (i == ARRAY_SIZE(ieee802154_table_pan_id_comp)) {
        TRACE(TR_DROP, "drop %-9s: unsupported address mode", "15.4");
        return -ENOTSUP;
    }

    if (ieee802154_table_pan_id_comp[i].dst_pan_id)
        iobuf_pop_le16(&iobuf);

    if (dst_addr_mode == MAC_ADDR_MODE_64_BIT) {
        iobuf_pop_le64(&iobuf);
    } else if (dst_addr_mode != MAC_ADDR_MODE_NONE) {
        TRACE(TR_DROP, "drop %-9s: unsupported address mode", "15.4");
        return -ENOTSUP;
    }

    if (ieee802154_table_pan_id_comp[i].src_pan_id)
        iobuf_pop_le16(&iobuf);

    if (src_addr_mode == MAC_ADDR_MODE_64_BIT) {
        iobuf_pop_le64(&iobuf);
    } else if (src_addr_mode != MAC_ADDR_MODE_NONE) {
        TRACE(TR_DROP, "drop %-9s: unsupported address mode", "15.4");
        return -ENOTSUP;
    }

    if (FIELD_GET(IEEE802154_FCF_SECURITY_ENABLED, fcf)) {
        ret = wsbr_data_sec_parse(&iobuf, &cnf->sec);
        if (ret < 0)
            return ret;
    } else {
        memset(&cnf->sec, 0, sizeof(cnf->sec));
    }

    if (FIELD_GET(IEEE802154_FCF_IE_PRESENT, fcf)) {
        ret = wsbr_data_ie_parse(&iobuf, ie);
        if (ret < 0)
            return ret;
    }

    if (iobuf_remaining_size(&iobuf))
        TRACE(TR_IGNORE, "ignore %-9s: unsupported frame payload", "15.4");

    if (iobuf.err) {
        TRACE(TR_DROP, "drop %-9s: malformed packet", "15.4");
        return -EINVAL;
    }

    return 0;
}

void wsbr_data_req_rebuild(struct iobuf_write *frame,
                           const struct rcp *rcp,
                           const struct mcps_data_req *req,
                           const struct mcps_data_req_ie_list *ie,
                           uint16_t pan_id)
{
    uint8_t tmp[8];
    uint16_t fcf;
    int i;

    BUG_ON(!ie);
    BUG_ON(req->msduLength);
    fcf = 0;
    fcf |= FIELD_PREP(IEEE802154_FCF_FRAME_TYPE,         IEEE802154_FRAME_TYPE_DATA);
    fcf |= FIELD_PREP(IEEE802154_FCF_SECURITY_ENABLED,   !!req->Key.SecurityLevel);
    fcf |= FIELD_PREP(IEEE802154_FCF_FRAME_PENDING,      req->PendingBit);
    fcf |= FIELD_PREP(IEEE802154_FCF_ACK_REQ,            req->TxAckReq);
    fcf |= FIELD_PREP(IEEE802154_FCF_PAN_ID_COMPRESSION, req->PanIdSuppressed);
    fcf |= FIELD_PREP(IEEE802154_FCF_SEQ_NUM_SUPPR,      req->SeqNumSuppressed);
    fcf |= FIELD_PREP(IEEE802154_FCF_IE_PRESENT,         ie->headerIovLength || ie->payloadIovLength);
    fcf |= FIELD_PREP(IEEE802154_FCF_DST_ADDR_MODE,      req->DstAddrMode);
    fcf |= FIELD_PREP(IEEE802154_FCF_FRAME_VERSION,      MAC_FRAME_VERSION_2015);
    fcf |= FIELD_PREP(IEEE802154_FCF_SRC_ADDR_MODE,      req->SrcAddrMode);
    iobuf_push_le16(frame, fcf);
    if (!req->SeqNumSuppressed)
        iobuf_push_data_reserved(frame, 1); // Sequence number

    for (i = 0; i < ARRAY_SIZE(ieee802154_table_pan_id_comp); i++)
        if (ieee802154_table_pan_id_comp[i].dst_addr_mode      == req->DstAddrMode &&
            ieee802154_table_pan_id_comp[i].src_addr_mode      == req->SrcAddrMode &&
            ieee802154_table_pan_id_comp[i].pan_id_compression == req->PanIdSuppressed)
            break;
    BUG_ON(i == ARRAY_SIZE(ieee802154_table_pan_id_comp), "invalid address mode");
    if (ieee802154_table_pan_id_comp[i].dst_pan_id)
        iobuf_push_le16(frame, req->DstPANId);
    if (req->DstAddrMode == MAC_ADDR_MODE_64_BIT) {
        memrcpy(tmp, req->DstAddr, 8);
        iobuf_push_data(frame, tmp, 8);
    } else if (req->DstAddrMode == MAC_ADDR_MODE_16_BIT) {
        memrcpy(tmp, req->DstAddr, 2);
        iobuf_push_data(frame, tmp, 2);
    }

    if (ieee802154_table_pan_id_comp[i].src_pan_id)
        iobuf_push_le16(frame, pan_id);
    if (req->SrcAddrMode == MAC_ADDR_MODE_64_BIT) {
        memrcpy(tmp, rcp->eui64, 8);
        iobuf_push_data(frame, tmp, 8);
    } else if (req->SrcAddrMode == MAC_ADDR_MODE_16_BIT) {
        BUG("unsupported");
    }

    if (req->Key.SecurityLevel) {
        iobuf_push_u8(frame, FIELD_PREP(IEEE802154_SECURITY_KEY_MODE, MAC_KEY_ID_MODE_IDX) |
                             FIELD_PREP(IEEE802154_SECURITY_LEVEL, req->Key.SecurityLevel));
        iobuf_push_data_reserved(frame, 4);  // Frame counter (never suppressed)
        iobuf_push_u8(frame, req->Key.KeyIndex);
    }

    if (ie->headerIovLength > 0)
        iobuf_push_data(frame, ie->headerIeVectorList[0].iov_base, ie->headerIeVectorList[0].iov_len);
    BUG_ON(ie->headerIovLength > 1);

    if (ie->payloadIovLength)
        iobuf_push_le16(frame, IEEE802154_IE_HT1);

    if (ie->payloadIovLength > 0)
        iobuf_push_data(frame, ie->payloadIeVectorList[0].iov_base, ie->payloadIeVectorList[0].iov_len);
    if (ie->payloadIovLength > 1)
        iobuf_push_data(frame, ie->payloadIeVectorList[1].iov_base, ie->payloadIeVectorList[1].iov_len);
    BUG_ON(ie->payloadIovLength > 2);

    // MIC
    if (req->Key.SecurityLevel == SEC_MIC32 || req->Key.SecurityLevel == SEC_ENC_MIC32)
        iobuf_push_data_reserved(frame, 4);
    if (req->Key.SecurityLevel == SEC_MIC64 || req->Key.SecurityLevel == SEC_ENC_MIC64)
        iobuf_push_data_reserved(frame, 8);
    if (req->Key.SecurityLevel == SEC_MIC128 || req->Key.SecurityLevel == SEC_ENC_MIC128)
        iobuf_push_data_reserved(frame, 16);
}
