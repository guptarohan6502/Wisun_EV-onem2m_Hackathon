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
#ifndef RCP_API_H
#define RCP_API_H

#include <stdbool.h>
#include <stdint.h>
#include "common/bus.h"
#include "common/hif.h"

// TODO: drop legacy structures
struct fhss_ws_neighbor_timing_info;
struct ws_fhss_config;
struct mcps_data_cnf;
struct mcps_data_ind;
struct mcps_data_rx_ie_list;
struct bus;
struct phy_rf_channel_configuration;
struct wsbr_ctxt;

struct rcp_rail_config {
    int      index;
    uint32_t chan0_freq;
    uint32_t chan_spacing;
    uint16_t chan_count;
    uint8_t  rail_phy_mode_id;
    int      phy_mode_group;
};

struct rcp {
    struct bus bus;

    void (*on_reset)(struct rcp *rcp);
    void (*on_tx_cnf)(struct rcp *rcp, const struct hif_tx_cnf *cnf);
    void (*on_rx_ind)(struct rcp *rcp, const struct hif_rx_ind *ind);

    bool has_reset;
    bool has_rf_list;
    uint32_t version_api;
    uint32_t version_fw;
    const char *version_label;
    uint8_t  eui64[8];
    struct rcp_rail_config *rail_config_list;
};

// Share rx buffer with legacy implementation to not allocate twice
extern uint8_t rcp_rx_buf[4096];

void rcp_rx(struct rcp *rcp);

void rcp_req_reset(struct rcp *rcp, bool bootload);
void rcp_set_host_api(struct rcp *rcp, uint32_t host_api_version);

void rcp_req_data_tx(struct rcp *rcp,
                     const uint8_t *frame, int frame_len,
                     uint8_t handle, uint8_t fhss_type,
                     const struct fhss_ws_neighbor_timing_info *fhss_data,
                     const uint32_t frame_counters_min[7],
                     const struct hif_rate_info rate_list[4], uint8_t ms_mode);
void rcp_req_data_tx_abort(struct rcp *rcp, uint8_t handle);

void rcp_req_radio_enable(struct rcp *rcp);
void rcp_req_radio_list(struct rcp *rcp);
void rcp_set_radio(struct rcp *rcp, uint8_t radioconf_index, uint8_t ofdm_mcs, bool enable_ms);
void rcp_set_radio_regulation(struct rcp *rcp, enum hif_reg reg);
void rcp_set_radio_tx_power(struct rcp *rcp, int8_t power_dbm);

void rcp_set_fhss_uc(struct rcp *rcp, const struct ws_fhss_config *cfg);
void rcp_set_fhss_ffn_bc(struct rcp *rcp, const struct ws_fhss_config *cfg);
void rcp_set_fhss_lfn_bc(struct rcp *rcp, const struct ws_fhss_config *cfg);
void rcp_set_fhss_async(struct rcp *rcp, const struct ws_fhss_config *cfg);

void rcp_set_sec_key(struct rcp *rcp,
                     uint8_t key_index,
                     const uint8_t key[16],
                     uint32_t frame_counter);

void rcp_set_filter_pan_id(struct rcp *rcp, uint16_t pan_id);
void rcp_set_filter_src64(struct rcp *rcp,
                          const uint8_t eui64[][8],
                          uint8_t count,
                          bool allow);
void rcp_set_filter_dst64(struct rcp *rcp, const uint8_t eui64[8]);

// Exported for wsbrd-fuzz
struct rcp_cmd {
    uint8_t cmd;
    void (*fn)(struct rcp *rcp, struct iobuf_read *buf);
};
extern struct rcp_cmd rcp_cmd_table[];

#endif
