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
#ifndef WSBR_COMMANDLINE_H
#define WSBR_COMMANDLINE_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <sys/socket.h> // Compatibility with linux headers < 4.12
#include <net/if.h>

#include "ws/ws_pae_controller.h"

// This struct is filled by parse_commandline() and never modified after.
struct wsbrd_conf {
    bool list_rf_configs;
    int color_output;

    char cpc_instance[PATH_MAX];

    char uart_dev[PATH_MAX];
    int  uart_baudrate;
    bool uart_rtscts;

    char tun_dev[IF_NAMESIZE];
    char neighbor_proxy[IF_NAMESIZE];
    bool tun_autoconf;
    bool internal_dhcp;

    char ws_name[33]; // null-terminated string of 32 chars
    int  ws_size;
    int  ws_domain;
    int  ws_mode;
    int  ws_class;
    int  ws_regional_regulation;
    int  ws_chan0_freq;
    int  ws_chan_spacing;
    int  ws_chan_count;
    uint8_t ws_allowed_channels[32];
    int  ws_phy_mode_id;
    int  ws_chan_plan_id;
    uint8_t ws_phy_op_modes[15];

    char user[LOGIN_NAME_MAX];
    char group[LOGIN_NAME_MAX];

    uint8_t ipv6_prefix[16];

    char capture[PATH_MAX];

    char storage_prefix[PATH_MAX];
    bool storage_delete;
    bool storage_exit;
    arm_certificate_entry_s tls_own;
    arm_certificate_entry_s tls_ca;
    uint8_t ws_gtk[4][16];
    bool ws_gtk_force[4];
    uint8_t ws_lgtk[4][16];
    bool ws_lgtk_force[4];
    struct sockaddr_storage radius_server;
    char radius_secret[256];

    int  tx_power;
    int  ws_pan_id;
    int  ws_fan_version;
    int  ws_pmk_lifetime_s;
    int  ws_ptk_lifetime_s;
    int  ws_gtk_expire_offset_s;
    int  ws_gtk_new_activation_time;
    int  ws_gtk_new_install_required;
    int  ws_ffn_revocation_lifetime_reduction;
    int  ws_lpmk_lifetime_s;
    int  ws_lptk_lifetime_s;
    int  ws_lgtk_expire_offset_s;
    int  ws_lgtk_new_activation_time;
    int  ws_lgtk_new_install_required;
    int  ws_lfn_revocation_lifetime_reduction;
    int  ws_async_frag_duration;
    int  uc_dwell_interval;
    int  bc_dwell_interval;
    int  bc_interval;
    int  lfn_bc_interval;
    int  lfn_bc_sync_period;
    bool enable_lfn;
    bool enable_ffn10;
    bool rpl_compat;
    bool rpl_rpi_ignorable;
    unsigned int ws_join_metrics;

    uint8_t ws_mac_address[8];
    uint8_t ws_allowed_mac_addresses[10][8];
    uint8_t ws_allowed_mac_address_count;
    uint8_t ws_denied_mac_addresses[10][8];
    uint8_t ws_denied_mac_address_count;

    int lowpan_mtu;
    int pan_size;
    char pcap_file[PATH_MAX];
};

void print_help_br(FILE *stream);

void parse_commandline(struct wsbrd_conf *config, int argc, char *argv[],
                       void (*print_help)(FILE *stream));

#endif
