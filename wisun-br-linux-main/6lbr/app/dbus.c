/*
 * SPDX-License-Identifier: LicenseRef-MSLA
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
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
#include <limits.h>
#include <sys/queue.h>
#include <arpa/inet.h>
#include <systemd/sd-bus.h>
#include <math.h>
#include "app/tun.h"
#include "common/string_extra.h"
#include "common/named_values.h"
#include "common/memutils.h"
#include "common/version.h"
#include "common/log.h"
#include "common/specs/ws.h"
#include "common/ns_list.h"
#include "common/mathutils.h"

#include "ws/ws_common.h"
#include "ws/ws_pae_controller.h"
#include "ws/ws_pae_key_storage.h"
#include "ws/ws_pae_lib.h"
#include "ws/ws_pae_auth.h"
#include "ws/ws_neigh.h"
#include "ws/ws_llc.h"
#include "net/protocol.h"
#include "security/protocols/sec_prot_keys.h"
#include "ipv6/ipv6_routing_table.h"

#include "commandline_values.h"
#include "wsbr.h"
#include "tun.h"

#include "dbus.h"

int dbus_set_mode_switch(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    uint8_t wisun_broadcast_mac_addr[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    struct wsbr_ctxt *ctxt = userdata;
    int ret;
    uint8_t *eui64;
    size_t eui64_len;
    int phy_mode_id;

    if (version_older_than(ctxt->rcp.version_api, 2, 0, 1))
        return sd_bus_error_set_errno(ret_error, ENOTSUP);

    sd_bus_message_read_array(m, 'y', (const void **)&eui64, &eui64_len);
    sd_bus_message_read_basic(m, 'i', &phy_mode_id);

    if (eui64_len == 0)
        eui64 = NULL;
    else if (eui64_len != 8)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    else if (!memcmp(eui64, wisun_broadcast_mac_addr, 8))
        eui64 = NULL;

    if (phy_mode_id > 0)
        ret = ws_llc_set_mode_switch(&ctxt->net_if, WS_MODE_SWITCH_PHY, phy_mode_id, eui64); // mode switch enabled
    else if (phy_mode_id == -1)
        ret = ws_llc_set_mode_switch(&ctxt->net_if, WS_MODE_SWITCH_DISABLED, 0, eui64); // mode switch disabled
    else if (phy_mode_id == 0)
        ret = ws_llc_set_mode_switch(&ctxt->net_if, WS_MODE_SWITCH_DEFAULT, 0, eui64); // mode switch back to default
    else
        ret = -EINVAL;

    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    sd_bus_reply_method_return(m, NULL);

    return 0;
}

int dbus_set_link_mode_switch(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    uint32_t phy_mode_id;
    size_t eui64_len;
    uint8_t ms_mode;
    uint8_t *eui64;
    int ret;

    sd_bus_message_read_array(m, 'y', (const void **)&eui64, &eui64_len);
    sd_bus_message_read_basic(m, 'u', &phy_mode_id);
    sd_bus_message_read_basic(m, 'y', &ms_mode);

    if (eui64_len == 0)
        eui64 = NULL;
    else if (eui64_len != 8)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    if (ms_mode > WS_MODE_SWITCH_MAC)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    if (ms_mode > WS_MODE_SWITCH_DISABLED && !phy_mode_id)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    if (ms_mode == WS_MODE_SWITCH_DEFAULT && phy_mode_id)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ret = ws_llc_set_mode_switch(&ctxt->net_if, ms_mode, phy_mode_id, eui64);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    sd_bus_reply_method_return(m, NULL);

    return 0;
}

int dbus_set_link_edfe(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    uint8_t edfe_mode;
    size_t eui64_len;
    uint8_t *eui64;
    int ret;

    sd_bus_message_read_array(m, 'y', (const void **)&eui64, &eui64_len);
    sd_bus_message_read_basic(m, 'y', &edfe_mode);

    if (eui64_len == 0)
        eui64 = NULL;
    else if (eui64_len != 8)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    if (edfe_mode >= WS_EDFE_MAX)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    if (edfe_mode == WS_EDFE_DEFAULT && !eui64)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    if (edfe_mode == WS_EDFE_ENABLED && version_older_than(ctxt->rcp.version_api, 2, 2, 0))
        return sd_bus_error_set_errno(ret_error, ENOTSUP);

    ret = ws_llc_set_edfe(&ctxt->net_if, edfe_mode, eui64);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    sd_bus_reply_method_return(m, NULL);

    return 0;
}

int dbus_join_multicast_group(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    const uint8_t *ipv6;
    size_t len;
    int ret;

    sd_bus_message_read_array(m, 'y', (const void **)&ipv6, &len);
    if (len != 16)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ret = wsbr_tun_join_mcast_group(ctxt->sock_mcast, ctxt->config.tun_dev, ipv6);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, errno);
    addr_add_group(&ctxt->net_if, ipv6);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

int dbus_leave_multicast_group(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    const uint8_t *ipv6;
    size_t len;
    int ret;

    sd_bus_message_read_array(m, 'y', (const void **)&ipv6, &len);
    if (len != 16)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ret = wsbr_tun_leave_mcast_group(ctxt->sock_mcast, ctxt->config.tun_dev, ipv6);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, errno);
    addr_remove_group(&ctxt->net_if, ipv6);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

void dbus_emit_keys_change(struct wsbr_ctxt *ctxt)
{
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Gtks", NULL);
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Gaks", NULL);
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Lgtks", NULL);
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Lgaks", NULL);
}

static int dbus_get_transient_keys(sd_bus_message *reply, struct net_if *net_if,
                                   sd_bus_error *ret_error, bool is_lfn)
{
    sec_prot_gtk_keys_t *gtks = ws_pae_controller_get_transient_keys(net_if->id, is_lfn);
    const int key_cnt = is_lfn ? LGTK_NUM : GTK_NUM;

    if (!gtks)
        return sd_bus_error_set_errno(ret_error, EBADR);
    sd_bus_message_open_container(reply, 'a', "ay");
    for (int i = 0; i < key_cnt; i++)
        sd_bus_message_append_array(reply, 'y', gtks->gtk[i].key, ARRAY_SIZE(gtks->gtk[i].key));
    sd_bus_message_close_container(reply);
    return 0;
}

static int dbus_get_gtks(sd_bus *bus, const char *path, const char *interface,
                         const char *property, sd_bus_message *reply,
                         void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_transient_keys(reply, (struct net_if *)userdata, ret_error, false);
}

static int dbus_get_lgtks(sd_bus *bus, const char *path, const char *interface,
                          const char *property, sd_bus_message *reply,
                          void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_transient_keys(reply, (struct net_if *)userdata, ret_error, true);
}

static int dbus_get_aes_keys(sd_bus_message *reply, struct net_if *net_if,
                             sd_bus_error *ret_error, bool is_lfn)
{
    sec_prot_gtk_keys_t *gtks = ws_pae_controller_get_transient_keys(net_if->id, is_lfn);
    const int key_cnt = is_lfn ? LGTK_NUM : GTK_NUM;
    uint8_t gak[16];

    if (!gtks)
        return sd_bus_error_set_errno(ret_error, EBADR);
    sd_bus_message_open_container(reply, 'a', "ay");
    for (int i = 0; i < key_cnt; i++) {
        // GAK is SHA256 of network name concatened with GTK
        ws_pae_controller_gak_from_gtk(gak, gtks->gtk[i].key, net_if->ws_info.network_name);
        sd_bus_message_append_array(reply, 'y', gak, ARRAY_SIZE(gak));
    }
    sd_bus_message_close_container(reply);
    return 0;
}

static int dbus_get_gaks(sd_bus *bus, const char *path, const char *interface,
                         const char *property, sd_bus_message *reply,
                         void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_aes_keys(reply, (struct net_if *)userdata, ret_error, false);
}

static int dbus_get_lgaks(sd_bus *bus, const char *path, const char *interface,
                          const char *property, sd_bus_message *reply,
                          void *userdata, sd_bus_error *ret_error)
{
    return dbus_get_aes_keys(reply, (struct net_if *)userdata, ret_error, true);
}

static int dbus_revoke_pairwise_keys(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    size_t eui64_len;
    uint8_t *eui64;
    int ret;

    sd_bus_message_read_array(m, 'y', (const void **)&eui64, &eui64_len);
    if (eui64_len != 8)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    ret = ws_pae_controller_node_keys_remove(ctxt->net_if.id, eui64);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_revoke_group_keys(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    uint8_t *gtk, *lgtk;
    size_t len;
    int ret;

    sd_bus_message_read_array(m, 'y', (const void **)&gtk, &len);
    if (!len)
        gtk = NULL;
    else if (len != GTK_LEN)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    sd_bus_message_read_array(m, 'y', (const void **)&lgtk, &len);
    if (!len)
        lgtk = NULL;
    else if (len != GTK_LEN)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ret = ws_pae_controller_node_access_revoke_start(ctxt->net_if.id, false, gtk);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    ret = ws_pae_controller_node_access_revoke_start(ctxt->net_if.id, true, lgtk);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_install_group_key(sd_bus_message *m, void *userdata,
                                  sd_bus_error *ret_error, bool is_lgtk)
{
    struct wsbr_ctxt *ctxt = userdata;
    const uint8_t *gtk;
    size_t len;

    sd_bus_message_read_array(m, 'y', (const void **)&gtk, &len);
    if (len != GTK_LEN)
        return sd_bus_error_set_errno(ret_error, EINVAL);

    ws_pae_auth_gtk_install(ctxt->net_if.id, gtk, is_lgtk);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_ie_custom_clear(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;

    ws_ie_custom_clear(&ctxt->net_if.ws_info.ie_custom_list);
    ws_mngt_pan_version_increase(&ctxt->net_if);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_ie_custom_insert(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    const uint8_t *frame_type_list;
    uint16_t frame_type_mask;
    size_t frame_type_count;
    uint8_t ie_type, ie_id;
    const uint8_t *content;
    size_t content_len;
    int ret;

    sd_bus_message_read(m, "yy", &ie_type, &ie_id);
    sd_bus_message_read_array(m, 'y', (const void **)&content, &content_len);
    sd_bus_message_read_array(m, 'y', (const void **)&frame_type_list, &frame_type_count);

    frame_type_mask = 0;
    for (size_t i = 0; i < frame_type_count; i++) {
        switch (frame_type_list[i]) {
        case WS_FT_PA:
        case WS_FT_PC:
        case WS_FT_EAPOL:
        case WS_FT_DATA:
        case WS_FT_LPA:
        case WS_FT_LPC:
            break;
        default:
            return sd_bus_error_set_errno(ret_error, -EINVAL);
        }
        frame_type_mask |= 1 << frame_type_list[i];
    }
    ret = ws_ie_custom_update(&ctxt->net_if.ws_info.ie_custom_list, ie_type, ie_id,
                              content, content_len, frame_type_mask);
    if (ret < 0)
        return sd_bus_error_set_errno(ret_error, -ret);
    ws_mngt_pan_version_increase(&ctxt->net_if);

    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_install_gtk(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    return dbus_install_group_key(m, userdata, ret_error, false);
}

static int dbus_install_lgtk(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    return dbus_install_group_key(m, userdata, ret_error, true);
}

int dbus_increment_rpl_dtsn(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;

    rpl_dtsn_inc(&ctxt->net_if.rpl_root);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

int dbus_increment_rpl_dodag_version_number(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;

    rpl_dodag_version_inc(&ctxt->net_if.rpl_root);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

static int dbus_set_filter_src64(sd_bus_message *m, void *userdata, sd_bus_error *ret_error, bool allow)
{
    struct wsbr_ctxt *ctxt = userdata;
    struct iobuf_write buf = { };
    const uint8_t *eui64;
    size_t eui64_len;

    sd_bus_message_enter_container(m, 'a', "ay");
    while (sd_bus_message_read_array(m, 'y', (const void **)&eui64, &eui64_len) > 0) {
        if (eui64_len != 8) {
            iobuf_free(&buf);
            return sd_bus_error_set_errno(ret_error, EINVAL);
        }
        iobuf_push_data(&buf, eui64, eui64_len);
    }
    sd_bus_message_close_container(m);

    // When given an empty list, 'allow' must be reversed
    rcp_set_filter_src64(&ctxt->rcp, (uint8_t (*)[8])buf.data, buf.len / 8, buf.len ? allow : !allow);
    iobuf_free(&buf);
    sd_bus_reply_method_return(m, NULL);
    return 0;
}

int dbus_allow_mac64(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    return dbus_set_filter_src64(m, userdata, ret_error, true);
}

int dbus_deny_mac64(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    return dbus_set_filter_src64(m, userdata, ret_error, false);
}

void dbus_emit_nodes_change(struct wsbr_ctxt *ctxt)
{
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "Nodes", NULL);
}

void dbus_emit_routing_graph_change(struct wsbr_ctxt *ctxt)
{
    sd_bus_emit_properties_changed(ctxt->dbus,
                       "/com/silabs/Wisun/BorderRouter",
                       "com.silabs.Wisun.BorderRouter",
                       "RoutingGraph", NULL);
}

static void dbus_message_open_info(sd_bus_message *m, const char *property,
                                  const char *name, const char *type)
{
    sd_bus_message_open_container(m, 'e', "sv");
    sd_bus_message_append(m, "s", name);
    sd_bus_message_open_container(m, 'v', type);
}

static void dbus_message_close_info(sd_bus_message *m, const char *property)
{
    sd_bus_message_close_container(m);
    sd_bus_message_close_container(m);
}

static int dbus_message_append_node(
    sd_bus_message *m,
    const char *property,
    const uint8_t self[8],
    bool is_br,
    supp_entry_t *supp,
    const struct ws_neigh *neighbor)
{
    int val;

    sd_bus_message_open_container(m, 'r', "aya{sv}");
    sd_bus_message_append_array(m, 'y', self, 8);
    sd_bus_message_open_container(m, 'a', "{sv}");
    {
        if (is_br) {
            dbus_message_open_info(m, property, "is_border_router", "b");
            sd_bus_message_append(m, "b", true);
            dbus_message_close_info(m, property);
            // TODO: deprecate is_border_router
            dbus_message_open_info(m, property, "node_role", "y");
            sd_bus_message_append(m, "y", WS_NR_ROLE_BR);
            dbus_message_close_info(m, property);
        } else if (supp) {
            dbus_message_open_info(m, property, "is_authenticated", "b");
            val = true;
            sd_bus_message_append(m, "b", val);
            dbus_message_close_info(m, property);
            if (ws_common_is_valid_nr(supp->sec_keys.node_role)) {
                dbus_message_open_info(m, property, "node_role", "y");
                sd_bus_message_append(m, "y", supp->sec_keys.node_role);
                dbus_message_close_info(m, property);
            }
        }
        if (neighbor) {
            dbus_message_open_info(m, property, "is_neighbor", "b");
            sd_bus_message_append(m, "b", true);
            dbus_message_close_info(m, property);
            if (neighbor->rx_power_dbm != INT_MAX) {
                dbus_message_open_info(m, property, "rssi", "y");
                sd_bus_message_append(m, "y", (uint8_t)(neighbor->rx_power_dbm + 174));
                dbus_message_close_info(m, property);
            } else if (neighbor->rx_power_dbm_unsecured != INT_MAX) {
                dbus_message_open_info(m, property, "rssi", "y");
                sd_bus_message_append(m, "y", (uint8_t)(neighbor->rx_power_dbm_unsecured + 174));
                dbus_message_close_info(m, property);
            }
            if (!isnan(neighbor->rsl_in_dbm)) {
                dbus_message_open_info(m, property, "rsl", "i");
                sd_bus_message_append(m, "i", (int)neighbor->rsl_in_dbm);
                dbus_message_close_info(m, property);
            } else if (!isnan(neighbor->rsl_in_dbm_unsecured)) {
                dbus_message_open_info(m, property, "rsl", "i");
                sd_bus_message_append(m, "i", (int)neighbor->rsl_in_dbm_unsecured);
                dbus_message_close_info(m, property);
            }
            if (!isnan(neighbor->rsl_out_dbm)) {
                dbus_message_open_info(m, property, "rsl_adv", "i");
                sd_bus_message_append(m, "i", (int)neighbor->rsl_out_dbm);
                dbus_message_close_info(m, property);
            }
            if (neighbor->lqi != INT_MAX) {
                dbus_message_open_info(m, property, "lqi", "y");
                sd_bus_message_append(m, "y", (uint8_t)neighbor->lqi);
                dbus_message_close_info(m, property);
            } else if (neighbor->lqi_unsecured != INT_MAX) {
                dbus_message_open_info(m, property, "lqi", "y");
                sd_bus_message_append(m, "y", (uint8_t)neighbor->lqi_unsecured);
                dbus_message_close_info(m, property);
            }
            dbus_message_open_info(m, property, "pom", "ay");
            sd_bus_message_append_array(m, 'y',
                                                neighbor->pom_ie.phy_op_mode_id,
                                                neighbor->pom_ie.phy_op_mode_number);
            dbus_message_close_info(m, property);

            dbus_message_open_info(m, property, "mdr_cmd_capable", "b");
            sd_bus_message_append(m, "b", neighbor->pom_ie.mdr_command_capable);
            dbus_message_close_info(m, property);
        }
    }
    sd_bus_message_close_container(m);
    sd_bus_message_close_container(m);
    return 0;
}

static const struct ws_neigh *dbus_get_neighbor_info(struct wsbr_ctxt *ctxt,
                                                               const uint8_t eui64[8])
{
    return ws_neigh_get(&ctxt->net_if.ws_info.neighbor_storage, eui64);
}

void dbus_message_append_node_br(sd_bus_message *m, const char *property, struct wsbr_ctxt *ctxt)
{
    struct ws_neigh neigh = {
        .rx_power_dbm = INT_MAX,
        .rx_power_dbm_unsecured = INT_MAX,
        .rsl_in_dbm  = NAN,
        .rsl_in_dbm_unsecured = NAN,
        .rsl_out_dbm = NAN,
        .lqi = INT_MAX,
        .lqi_unsecured = INT_MAX,
        .pom_ie.mdr_command_capable = true,
    };

    while (ctxt->net_if.ws_info.phy_config.phy_op_modes[neigh.pom_ie.phy_op_mode_number])
        neigh.pom_ie.phy_op_mode_number++;
    memcpy(neigh.pom_ie.phy_op_mode_id,
           ctxt->net_if.ws_info.phy_config.phy_op_modes,
           neigh.pom_ie.phy_op_mode_number);
    dbus_message_append_node(m, property, ctxt->rcp.eui64, true, false, &neigh);
}

int dbus_get_nodes(sd_bus *bus, const char *path, const char *interface,
                       const char *property, sd_bus_message *reply,
                       void *userdata, sd_bus_error *ret_error)
{
    const struct ws_neigh *neighbor_info;
    struct wsbr_ctxt *ctxt = userdata;
    int len_pae;
    uint8_t eui64_pae[4096][8];
    supp_entry_t *supp;

    len_pae = ws_pae_auth_supp_list(ctxt->net_if.id, eui64_pae, sizeof(eui64_pae));

    sd_bus_message_open_container(reply, 'a', "(aya{sv})");
    dbus_message_append_node_br(reply, property, ctxt);

    for (int i = 0; i < len_pae; i++) {
        neighbor_info = dbus_get_neighbor_info(ctxt, eui64_pae[i]);
        if (ws_pae_key_storage_supp_exists(eui64_pae[i]))
            supp = ws_pae_key_storage_supp_read(NULL, eui64_pae[i], NULL, NULL, NULL);
        else
            supp = NULL;
        dbus_message_append_node(reply, property, eui64_pae[i],
                                 false, supp, neighbor_info);
        if (supp)
            free(supp);
    }
    sd_bus_message_close_container(reply);
    return 0;
}

static void dbus_message_append_rpl_target(sd_bus_message *reply, struct rpl_target *target, uint8_t pcs)
{
    uint8_t j;

    sd_bus_message_open_container(reply, 'r', "aybaay");
    sd_bus_message_append_array(reply, 'y', target->prefix, 16);
    sd_bus_message_append(reply, "b", target->external);
    sd_bus_message_open_container(reply, 'a', "ay");
    for (uint8_t i = 0; i < pcs + 1; i++) {
        if (!memzcmp(target->transits + i, sizeof(struct rpl_transit)))
            continue;
        for (j = 0; j < i; j++)
            if (!memcmp(target->transits + i, target->transits + j, sizeof(struct rpl_transit)))
                break;
        if (i == j)
            sd_bus_message_append_array(reply, 'y', target->transits[i].parent, 16);
    }
    sd_bus_message_close_container(reply);
    sd_bus_message_close_container(reply);
}

static void dbus_message_append_ipv6_neigh(sd_bus_message *reply, struct ipv6_neighbour *ipv6_neigh, struct rpl_root *root)
{
    struct rpl_target target = { };

    memcpy(target.prefix, ipv6_neigh->ip_address, 16);
    target.external = true;
    memcpy(target.transits[0].parent, root->dodag_id, 16);
    dbus_message_append_rpl_target(reply, &target, root->pcs);
}

int dbus_get_routing_graph(sd_bus *bus, const char *path, const char *interface,
                           const char *property, sd_bus_message *reply,
                           void *userdata, sd_bus_error *ret_error)
{
    struct wsbr_ctxt *ctxt = userdata;
    struct rpl_target target_br = { };
    struct rpl_target *target;
    struct ws_neigh *ws_neigh;

    sd_bus_message_open_container(reply, 'a', "(aybaay)");

    tun_addr_get_global_unicast(ctxt->config.tun_dev, target_br.prefix);
    dbus_message_append_rpl_target(reply, &target_br, 0);

    SLIST_FOREACH(target, &ctxt->net_if.rpl_root.targets, link)
        dbus_message_append_rpl_target(reply, target, ctxt->net_if.rpl_root.pcs);

    // Since LFN are not routed by RPL, rank 1 LFNs are not RPL targets.
    // This hack allows to expose rank 1 LFNs and relies on their ipv6 address
    // registration.
    ns_list_foreach(struct ipv6_neighbour, ipv6_neigh, &ctxt->net_if.ipv6_neighbour_cache.list) {
        if (IN6_IS_ADDR_MULTICAST(ipv6_neigh->ip_address) || IN6_IS_ADDR_LINKLOCAL(ipv6_neigh->ip_address))
            continue;
        if (rpl_target_get(&ctxt->net_if.rpl_root, ipv6_neigh->ip_address))
            continue;
        ws_neigh = ws_neigh_get(&ctxt->net_if.ws_info.neighbor_storage,
                                ipv6_neighbour_eui64(&ctxt->net_if.ipv6_neighbour_cache, ipv6_neigh));
        if (!ws_neigh || ws_neigh->node_role != WS_NR_ROLE_LFN)
            continue;
        dbus_message_append_ipv6_neigh(reply, ipv6_neigh, &ctxt->net_if.rpl_root);
    }

    sd_bus_message_close_container(reply);
    return 0;
}

int dbus_get_hw_address(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    uint8_t *hw_addr = userdata;

    sd_bus_message_append_array(reply, 'y', hw_addr, 8);
    return 0;
}

int dbus_get_ws_pan_id(sd_bus *bus, const char *path, const char *interface,
                       const char *property, sd_bus_message *reply,
                       void *userdata, sd_bus_error *ret_error)
{
    struct net_if *net_if = protocol_stack_interface_info_get_by_id(*(int *)userdata);

    if (!net_if)
        return sd_bus_error_set_errno(ret_error, EINVAL);
    sd_bus_message_append(reply, "q", net_if->ws_info.pan_information.pan_id);
    return 0;
}

int dbus_get_fan_version(sd_bus *bus, const char *path, const char *interface,
                         const char *property, sd_bus_message *reply,
                         void *userdata, sd_bus_error *ret_error)
{
    struct net_if *net_if = protocol_stack_interface_info_get_by_id(*(int *)userdata);
    uint8_t fan_version = net_if->ws_info.pan_information.version;

    sd_bus_message_append_basic(reply, 'y', &fan_version);
    return 0;
}

int wsbrd_get_ws_domain(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    int *domain = userdata;

    sd_bus_message_append(reply, "s", val_to_str(*domain, valid_ws_domains, "[unknown]"));
    return 0;
}

int wsbrd_get_ws_size(sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
    int *size = userdata;

    sd_bus_message_append(reply, "s", val_to_str(*size, valid_ws_size, NULL));
    return 0;
}

int dbus_get_string(sd_bus *bus, const char *path, const char *interface,
               const char *property, sd_bus_message *reply,
               void *userdata, sd_bus_error *ret_error)
{
    char *val = userdata;

    sd_bus_message_append(reply, "s", val);
    return 0;
}

static const sd_bus_vtable dbus_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("JoinMulticastGroup",  "ay",     NULL, dbus_join_multicast_group,  0),
        SD_BUS_METHOD("LeaveMulticastGroup", "ay",     NULL, dbus_leave_multicast_group, 0),
        SD_BUS_METHOD("SetModeSwitch",       "ayi",    NULL, dbus_set_mode_switch,       SD_BUS_VTABLE_DEPRECATED),
        SD_BUS_METHOD("SetLinkModeSwitch",   "ayuy",   NULL, dbus_set_link_mode_switch,  0),
        SD_BUS_METHOD("SetLinkEdfe",         "ayy",    NULL, dbus_set_link_edfe,         0),
        SD_BUS_METHOD("RevokePairwiseKeys",  "ay",     NULL, dbus_revoke_pairwise_keys,  0),
        SD_BUS_METHOD("RevokeGroupKeys",     "ayay",   NULL, dbus_revoke_group_keys,     0),
        SD_BUS_METHOD("InstallGtk",          "ay",     NULL, dbus_install_gtk,           0),
        SD_BUS_METHOD("InstallLgtk",         "ay",     NULL, dbus_install_lgtk,          0),
        SD_BUS_METHOD("IeCustomInsert",      "yyayay", NULL, dbus_ie_custom_insert,      0),
        SD_BUS_METHOD("IeCustomClear",       NULL,     NULL, dbus_ie_custom_clear,       0),
        SD_BUS_METHOD("IncrementRplDtsn",    NULL,     NULL, dbus_increment_rpl_dtsn,    0),
        SD_BUS_METHOD("IncrementRplDodagVersionNumber", NULL, NULL, dbus_increment_rpl_dodag_version_number, 0),
        SD_BUS_METHOD("AllowMac64",          "aay",    NULL, dbus_allow_mac64, 0),
        SD_BUS_METHOD("DenyMac64",           "aay",    NULL, dbus_deny_mac64, 0),
        SD_BUS_PROPERTY("Gtks", "aay", dbus_get_gtks,
                        offsetof(struct wsbr_ctxt, net_if),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Gaks", "aay", dbus_get_gaks,
                        offsetof(struct wsbr_ctxt, net_if),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Lgtks", "aay", dbus_get_lgtks,
                        offsetof(struct wsbr_ctxt, net_if),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Lgaks", "aay", dbus_get_lgaks,
                        offsetof(struct wsbr_ctxt, net_if),
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
        SD_BUS_PROPERTY("Nodes", "a(aya{sv})", dbus_get_nodes, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION),
        SD_BUS_PROPERTY("RoutingGraph", "a(aybaay)", dbus_get_routing_graph, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION),
        SD_BUS_PROPERTY("HwAddress", "ay", dbus_get_hw_address,
                        offsetof(struct wsbr_ctxt, rcp.eui64),
                        0),
        SD_BUS_PROPERTY("WisunNetworkName", "s", dbus_get_string,
                        offsetof(struct wsbr_ctxt, config.ws_name),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunSize", "s", wsbrd_get_ws_size,
                        offsetof(struct wsbr_ctxt, config.ws_size),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunDomain", "s", wsbrd_get_ws_domain,
                        offsetof(struct wsbr_ctxt, config.ws_domain),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunMode", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_mode),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunClass", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_class),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunPhyModeId", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_phy_mode_id),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunChanPlanId", "u", NULL,
                        offsetof(struct wsbr_ctxt, config.ws_chan_plan_id),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunPanId", "q", dbus_get_ws_pan_id,
                        offsetof(struct wsbr_ctxt, net_if.id),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_PROPERTY("WisunFanVersion", "y", dbus_get_fan_version,
                        offsetof(struct wsbr_ctxt, net_if.id),
                        SD_BUS_VTABLE_PROPERTY_CONST),
        SD_BUS_VTABLE_END
};

void dbus_register(struct wsbr_ctxt *ctxt)
{
    int ret;
    char mode = 'A';
    const char *env_var;
    const char *dbus_scope = "undefined";

    env_var = getenv("DBUS_STARTER_BUS_TYPE");
    if (env_var && !strcmp(env_var, "system"))
        mode = 'S';
    if (env_var && !strcmp(env_var, "user"))
        mode = 'U';
    if (env_var && !strcmp(env_var, "session"))
        mode = 'U';
    if (mode == 'U' || mode == 'A')
        ret = sd_bus_default_user(&ctxt->dbus);
    if (mode == 'S' || (mode == 'A' && ret < 0))
        ret = sd_bus_default_system(&ctxt->dbus);
    if (ret < 0) {
        WARN("DBus not available: %s", strerror(-ret));
        return;
    }

    ret = sd_bus_add_object_vtable(ctxt->dbus, NULL, "/com/silabs/Wisun/BorderRouter",
                                   "com.silabs.Wisun.BorderRouter",
                                   dbus_vtable, ctxt);
    if (ret < 0) {
        WARN("%s: %s", __func__, strerror(-ret));
        return;
    }

    ret = sd_bus_request_name(ctxt->dbus, "com.silabs.Wisun.BorderRouter",
                              SD_BUS_NAME_ALLOW_REPLACEMENT | SD_BUS_NAME_REPLACE_EXISTING);
    if (ret < 0) {
        WARN("%s: %s", __func__, strerror(-ret));
        return;
    }

    sd_bus_get_scope(ctxt->dbus, &dbus_scope);
    INFO("Successfully registered to %s DBus", dbus_scope);
}

int dbus_process(struct wsbr_ctxt *ctxt)
{
    BUG_ON(!ctxt->dbus);
    sd_bus_process(ctxt->dbus, NULL);
    return 0;
}

int dbus_get_fd(struct wsbr_ctxt *ctxt)
{
    if (ctxt->dbus)
        return sd_bus_get_fd(ctxt->dbus);
    else
        return -1;
}
