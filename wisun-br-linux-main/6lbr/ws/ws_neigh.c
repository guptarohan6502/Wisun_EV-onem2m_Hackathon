/*
 * Copyright (c) 2018-2020, Pelion and affiliates.
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <inttypes.h>
#include <limits.h>
#include "common/sys_queue_extra.h"
#include "common/string_extra.h"
#include "common/time_extra.h"
#include "common/ws_regdb.h"
#include "common/version.h"
#include "common/endian.h"
#include "common/mathutils.h"
#include "common/memutils.h"
#include "common/rand.h"
#include "common/log.h"
#include "common/bits.h"
#include "common/specs/ws.h"

#include "6lbr/ws/ws_common.h"


#include "ws_neigh.h"

#define LFN_SCHEDULE_GUARD_TIME_MS 300

struct ws_neigh *ws_neigh_add(struct ws_neigh_table *table,
                         const uint8_t mac64[8],
                         uint8_t role, int8_t tx_power_dbm,
                         unsigned int key_index_mask)
{
    struct ws_neigh *neigh = zalloc(sizeof(struct ws_neigh));

    neigh->node_role = role;
    for (uint8_t key_index = 1; key_index <= 7; key_index++)
        if (!(key_index_mask & (1u << key_index)))
            neigh->frame_counter_min[key_index - 1] = UINT32_MAX;
    memcpy(neigh->mac64, mac64, 8);
    neigh->lifetime_s = WS_NEIGHBOUR_TEMPORARY_ENTRY_LIFETIME;
    neigh->expiration_s = time_current(CLOCK_MONOTONIC) + WS_NEIGHBOUR_TEMPORARY_ENTRY_LIFETIME;
    neigh->rsl_in_dbm = NAN;
    neigh->rsl_in_dbm_unsecured = NAN;
    neigh->rsl_out_dbm = NAN;
    neigh->rx_power_dbm = INT_MAX;
    neigh->rx_power_dbm_unsecured = INT_MAX;
    neigh->lqi = INT_MAX;
    neigh->lqi_unsecured = INT_MAX;
    neigh->apc_txpow_dbm = tx_power_dbm;
    neigh->apc_txpow_dbm_ofdm = tx_power_dbm;
    SLIST_INSERT_HEAD(&table->neigh_list, neigh, link);
    TRACE(TR_NEIGH_15_4, "15.4 neighbor add %s / %ds", tr_eui64(neigh->mac64), neigh->lifetime_s);
    return neigh;
}

struct ws_neigh *ws_neigh_get(struct ws_neigh_table *table, const uint8_t *mac64)
{
    struct ws_neigh *neigh;

    SLIST_FOREACH(neigh, &table->neigh_list, link)
        if (!memcmp(neigh->mac64, mac64, 8))
            return neigh;

    return NULL;
}

void ws_neigh_del(struct ws_neigh_table *table, const uint8_t *mac64)
{
    struct ws_neigh *neigh = ws_neigh_get(table, mac64);

    if (neigh) {
        SLIST_REMOVE(&table->neigh_list, neigh, ws_neigh, link);
        TRACE(TR_NEIGH_15_4, "15.4 neighbor del %s / %ds", tr_eui64(neigh->mac64), neigh->lifetime_s);
        free(neigh);
    }
}

void ws_neigh_table_expire(struct ws_neigh_table *table, int time_update)
{
    struct ws_neigh *neigh;
    struct ws_neigh *tmp;

    SLIST_FOREACH_SAFE(neigh, &table->neigh_list, link, tmp)
        if (time_current(CLOCK_MONOTONIC) >= neigh->expiration_s)
            if (table->on_expire)
                table->on_expire(neigh->mac64);
}

size_t ws_neigh_get_neigh_count(struct ws_neigh_table *table)
{
    return SLIST_SIZE(&table->neigh_list, link);
}

static void ws_neigh_calculate_ufsi_drift(struct fhss_ws_neighbor_timing_info *fhss_data, uint24_t ufsi,
                                          uint64_t timestamp, const uint8_t address[8])
{
    if (fhss_data->ffn.utt_rx_tstamp_us && fhss_data->ffn.ufsi) {
        // No UFSI on fixed channel
        if (fhss_data->uc_chan_func == WS_CHAN_FUNC_FIXED) {
            return;
        }
        double seq_length = 0x10000;
        if (fhss_data->uc_chan_func == WS_CHAN_FUNC_TR51CF) {
            seq_length = fhss_data->uc_chan_count;
        }
        double ufsi_prev_tmp = fhss_data->ffn.ufsi;
        double ufsi_cur_tmp = ufsi;
        if (fhss_data->uc_chan_func == WS_CHAN_FUNC_DH1CF) {
            if (ufsi_cur_tmp < ufsi_prev_tmp) {
                ufsi_cur_tmp += 0xffffff;
            }
        }
        // Convert 24-bit UFSI to real time before drift calculation
        double time_since_seq_start_prev_ms = (ufsi_prev_tmp * seq_length * fhss_data->ffn.uc_dwell_interval_ms) / 0x1000000;
        double time_since_seq_start_cur_ms = (ufsi_cur_tmp * seq_length * fhss_data->ffn.uc_dwell_interval_ms) / 0x1000000;
        uint64_t time_since_last_ufsi_us = timestamp - fhss_data->ffn.utt_rx_tstamp_us;

        if (fhss_data->uc_chan_func == WS_CHAN_FUNC_TR51CF) {
            uint32_t full_uc_schedule_ms = fhss_data->ffn.uc_dwell_interval_ms * fhss_data->uc_chan_count;
            uint32_t temp_ms;

            if (!full_uc_schedule_ms)
                return;
            temp_ms = (time_since_last_ufsi_us / 1000) / full_uc_schedule_ms;
            if (time_since_seq_start_cur_ms >= time_since_seq_start_prev_ms) {
                temp_ms--;
            }
            time_since_seq_start_cur_ms += temp_ms * full_uc_schedule_ms + (full_uc_schedule_ms - time_since_seq_start_prev_ms) + time_since_seq_start_prev_ms;
        }

        double ufsi_diff_ms = time_since_seq_start_cur_ms - time_since_seq_start_prev_ms;
        if (time_since_seq_start_cur_ms < time_since_seq_start_prev_ms)
            // add ufsi sequence length
            ufsi_diff_ms += seq_length * fhss_data->ffn.uc_dwell_interval_ms;

        double ufsi_drift_ms = time_since_last_ufsi_us / 1000.f - ufsi_diff_ms;
        // Since resolution of the RCP timer is 1µs, a window 10 million times
        // larger (=10s) allows to get 0.1ppm of precision in the calculus below
        // FIXME: improve precision by storing ufsi over time and calculate drift
        // over a bigger window
        if (time_since_last_ufsi_us >= 10000000)
            TRACE(TR_NEIGH_15_4, "15.4 neighbor sync %s / %.01lfppm drift (%.0lfus in %"PRId64"s)", tr_eui64(address),
                  1000000000.f * ufsi_drift_ms / time_since_last_ufsi_us, ufsi_drift_ms * 1000, time_since_last_ufsi_us / 1000000);
        else
            TRACE(TR_NEIGH_15_4, "15.4 neighbor sync %s / drift measure not available", tr_eui64(address));
    }
}

void ws_neigh_ut_update(struct fhss_ws_neighbor_timing_info *fhss_data, uint24_t ufsi,
                        uint64_t tstamp_us, const uint8_t eui64[8])
{
    ws_neigh_calculate_ufsi_drift(fhss_data, ufsi, tstamp_us, eui64);

    if (fhss_data->ffn.utt_rx_tstamp_us == tstamp_us &&
        fhss_data->ffn.ufsi             == ufsi)
        return; // Save an update

    fhss_data->ffn.utt_rx_tstamp_us = tstamp_us;
    fhss_data->ffn.ufsi             = ufsi;
}

// Wi-SUN FAN 1.1v08 - 6.3.4.6.4.2.6 Maintaining FFN / LFN Synchronization
//   When the FFN receives a LUTT-IE from a LFN it does not adjust any time
//   difference relative to the expected LFN’s unicast listening reference point.
// In fact, the LUTT information must only be updated when combined with an
// LUS-IE which indicates a change in timing offset and/or interval.
void ws_neigh_lut_update(struct fhss_ws_neighbor_timing_info *fhss_data,
                         uint16_t slot_number, uint24_t interval_offset,
                         uint64_t tstamp_us, const uint8_t eui64[8])
{
    fhss_data->lfn.lutt_rx_tstamp_us     = tstamp_us;
    fhss_data->lfn.uc_slot_number        = slot_number;
    fhss_data->lfn.uc_interval_offset_ms = interval_offset;
}

void ws_neigh_lnd_update(struct fhss_ws_neighbor_timing_info *fhss_data, const struct ws_lnd_ie *ie_lnd, uint64_t tstamp_us)
{
    fhss_data->lfn.lpa_response_delay_ms = ie_lnd->response_delay;
    fhss_data->lfn.lpa_slot_duration_ms  = ie_lnd->discovery_slot_time;
    fhss_data->lfn.lpa_slot_count        = ie_lnd->discovery_slots;
    fhss_data->lfn.lpa_slot_first        = ie_lnd->discovery_first_slot;
    fhss_data->lfn.lnd_rx_tstamp_us      = tstamp_us;
}

void ws_neigh_nr_update(struct ws_neigh *neigh, struct ws_nr_ie *nr_ie)
{
    neigh->lto_info.uc_interval_min_ms = nr_ie->listen_interval_min;
    neigh->lto_info.uc_interval_max_ms = nr_ie->listen_interval_max;
}

static void ws_neigh_excluded_mask_by_range(uint8_t channel_mask[32],
                                            const struct ws_excluded_channel_range *range_info,
                                            uint16_t number_of_channels)
{
    uint16_t range_start, range_stop;
    const uint8_t *range_ptr = range_info->range_start;

    for (int i = 0; i < range_info->number_of_range; i++) {
        range_start = read_le16(range_ptr);
        range_ptr += 2;
        range_stop = MIN(read_le16(range_ptr), number_of_channels);
        range_ptr += 2;
        bitfill(channel_mask, false, range_start, range_stop);
    }
}

static void ws_neigh_excluded_mask_by_mask(uint8_t channel_mask[32],
                                           const struct ws_excluded_channel_mask *mask_info,
                                           uint16_t number_of_channels)
{
    int nchan = MIN(number_of_channels, mask_info->mask_len_inline * 8);

    for (int i = 0; i < nchan; i++)
        if (bittest(mask_info->channel_mask, i))
            bitclr(channel_mask, i);
}

static void ws_neigh_set_chan_list(const struct ws_fhss_config *fhss_config,
                                   uint8_t chan_mask[32],
                                   const struct ws_generic_channel_info *chan_info,
                                   uint16_t *chan_cnt)
{
    const struct chan_params *params = NULL;

    switch (chan_info->channel_plan) {
    case 0:
        params = ws_regdb_chan_params(chan_info->plan.zero.regulatory_domain, 0, chan_info->plan.zero.operating_class);
        BUG_ON(!params);
        *chan_cnt = params->chan_count;
        break;
    case 1:
        *chan_cnt = chan_info->plan.one.number_of_channel;
        break;
    case 2:
        params = ws_regdb_chan_params(chan_info->plan.two.regulatory_domain, chan_info->plan.two.channel_plan_id, 0);
        BUG_ON(!params);
        *chan_cnt = params->chan_count;
        break;
    default:
        BUG("unsupported channel plan: %d", chan_info->channel_plan);
    }

    if (params)
        ws_common_generate_channel_list(chan_mask, params->chan_count,
                                        fhss_config->regional_regulation,
                                        params->reg_domain, params->op_class, params->chan_plan_id);
    else
        ws_common_generate_channel_list(chan_mask, chan_info->plan.one.number_of_channel,
                                        fhss_config->regional_regulation,
                                        REG_DOMAIN_UNDEF, 0, 0);

    if (chan_info->excluded_channel_ctrl == WS_EXC_CHAN_CTRL_RANGE)
        ws_neigh_excluded_mask_by_range(chan_mask, &chan_info->excluded_channels.range, *chan_cnt);
    if (chan_info->excluded_channel_ctrl == WS_EXC_CHAN_CTRL_BITMASK)
        ws_neigh_excluded_mask_by_mask(chan_mask, &chan_info->excluded_channels.mask, *chan_cnt);
}

void ws_neigh_us_update(const struct ws_fhss_config *fhss_config, struct fhss_ws_neighbor_timing_info *fhss_data,
                        const struct ws_generic_channel_info *chan_info,
                        uint8_t dwell_interval, const uint8_t eui64[8])
{
    fhss_data->uc_chan_func = chan_info->channel_function;
    if (chan_info->channel_function == WS_CHAN_FUNC_FIXED) {
        fhss_data->uc_chan_fixed = chan_info->function.zero.fixed_channel;
        fhss_data->uc_chan_count = 1;
    } else {
        ws_neigh_set_chan_list(fhss_config, fhss_data->uc_channel_list, chan_info,
                                  &fhss_data->uc_chan_count);
    }
    fhss_data->ffn.uc_dwell_interval_ms = dwell_interval;
}

bool ws_neigh_has_us(const struct fhss_ws_neighbor_timing_info *fhss_data)
{
    return memzcmp(fhss_data->uc_channel_list, sizeof(fhss_data->uc_channel_list));
}

// Compute the divisors of val closest to q_ref, possibly including 1 and val
static void ws_neigh_calc_closest_divisors(uint24_t val, uint24_t q_ref,
                                           uint24_t *below, uint24_t *above)
{
    uint24_t q;
    uint24_t _q;

    *below = 0;
    *above = 0;
    // Iterate through divisors from 1 to sqrt(val)
    for (q = 1; q * q <= val; q++) {
        if (val % q == 0) {
            if (q <= q_ref) {
                *below = q;
            } else {
                *above = q;
                return;
            }
        }
    }
    // Iterate through the remaining divisors
    q--;
    for (; q > 0; q--) {
        _q = val / q;
        if (val % _q == 0) {
            if (_q <= q_ref) {
                *below = _q;
            } else {
                *above = _q;
                return;
            }
        }
    }
}

// Compute the Adjusted Listening Interval to be included in the LTO-IE
// See Wi-SUN FAN 1.1v06 6.3.4.6.4.2.1.2 FFN Processing of LFN PAN Advertisement Solicit
uint24_t ws_neigh_calc_lfn_adjusted_interval(uint24_t bc_interval, uint24_t uc_interval,
                                             uint24_t uc_interval_min, uint24_t uc_interval_max)
{
    uint24_t r;
    uint24_t q_above;
    uint24_t q_below;

    if (!bc_interval || !uc_interval || !uc_interval_min || !uc_interval_max)
        return 0;
    if (uc_interval < uc_interval_min || uc_interval > uc_interval_max) {
        TRACE(TR_IGNORE, "ignore: lto-ie incoherent with nr-ie");
        return 0;
    }

    if (uc_interval > bc_interval) {
        // Current state:
        //   uc = q * bc + r
        // Desired state:
        //   uc' = q' * bc
        // This can be solved arithmetically:
        //   for a bigger interval:  uc' = uc + bc - r = (q + 1) * bc
        //   for a smaller interval: uc' = uc - r = q * bc
        r = uc_interval % bc_interval;
        if (r == 0)
            return uc_interval; // No need to adjust
        if (uc_interval + bc_interval - r <= uc_interval_max)
            return uc_interval + bc_interval - r; // Extend interval
        if (uc_interval - r >= uc_interval_min)
            return uc_interval - r; // Reduce interval
        return uc_interval; // No multiple available in range
    } else {
        // Current state:
        //   bc = q * uc + r
        // Desired state:
        //   bc = q' * uc'
        // This case is much more difficult. The solution proposed here is
        // iterate through divisors of bc to find those closest to q:
        //   q_below <= q < q_above
        //   for a bigger interval:  uc' = bc / q_below
        //   for a smaller interval: uc' = bc / q_above
        if (bc_interval % uc_interval == 0)
            return uc_interval; // No need to adjust

        ws_neigh_calc_closest_divisors(bc_interval, bc_interval / uc_interval,
                                                &q_below, &q_above);

        if (q_above && bc_interval / q_above >= uc_interval_min)
            return bc_interval / q_above; // Reduce interval
        if (q_below && bc_interval / q_below <= uc_interval_max)
            return bc_interval / q_below; // Extend interval
        return uc_interval; // No sub-multiple available in range
    }
}

uint24_t ws_neigh_calc_lfn_offset(uint24_t adjusted_listening_interval, uint32_t bc_interval)
{
    /* This minimalist algorithm ensures that LFN BC will not overlap with any
     * LFN UC.
     * It returns an offset inside the LFN BC Interval that will be used by the
     * MAC to computed the actual offset to be applied by the targeted LFN.
     * Any LFN UC is placed randomly after the LFN BC, in an interval of
     *   offset = [GUARD_INTERVAL, LFN_BC_INTERVAL - GUARD_INTERVAL] or
     *   offset = [GUARD_INTERVAL, LFN_UC_INTERVAL - GUARD_INTERVAL]
     * For any multiple LFN UC interval, the listening slot will happen at
     * "offset + n*LFN_BC_INTERVAL" which guarantees that it will not come near
     * the LFN BC slot.
     * For any divisor LFN UC interval, the listening slot will happen an
     * entire number of times between two LFN BC slot, which is fine.
     * The two closest LFN UC slots are at:
     *   "offset + n*LFN_BC_INTERVAL - LFN_UC INTERVAL" and
     *   "offset + n*LFN_BC_INTERVAL"
     * These are safe as long as "LFN_UC_INTERVAL >= 2 * GUARD_INTERVAL"
     * Because of the randomness and the offset range depending on the
     * LFN UC Interval, there is no protection between LFN Unicast schedules.
     * However, they are spread as much as possible.
     * TODO: algorithm that allocates or reallocates offsets to each LFN in
     * order to minimize overlap.
     */
    uint16_t max_offset_ms;

    // Cannot protect LFN BC with such a short interval, do nothing
    if (adjusted_listening_interval < 2 * LFN_SCHEDULE_GUARD_TIME_MS)
        return 0;

    if (adjusted_listening_interval >= bc_interval)
        max_offset_ms = bc_interval - LFN_SCHEDULE_GUARD_TIME_MS;
    else
        max_offset_ms = adjusted_listening_interval - LFN_SCHEDULE_GUARD_TIME_MS;
    return LFN_SCHEDULE_GUARD_TIME_MS * rand_get_random_in_range(1, max_offset_ms / LFN_SCHEDULE_GUARD_TIME_MS);
}

bool ws_neigh_lus_update(const struct ws_fhss_config *fhss_config,
                         struct fhss_ws_neighbor_timing_info *fhss_data,
                         const struct ws_generic_channel_info *chan_info,
                         uint24_t listen_interval_ms, const struct lto_info *lto_info)
{
    uint24_t adjusted_listening_interval;
    bool offset_adjusted = true;

    if (fhss_data->lfn.uc_listen_interval_ms != listen_interval_ms) {
        adjusted_listening_interval = ws_neigh_calc_lfn_adjusted_interval(fhss_config->lfn_bc_interval,
                                                                          fhss_data->lfn.uc_listen_interval_ms,
                                                                          lto_info->uc_interval_min_ms,
                                                                          lto_info->uc_interval_max_ms);
        if (adjusted_listening_interval && adjusted_listening_interval != listen_interval_ms)
            offset_adjusted = false;
    }

    fhss_data->lfn.uc_listen_interval_ms = listen_interval_ms;
    if (!chan_info)
        return offset_adjusted; // Support chan plan tag 255 (reuse previous schedule)
    fhss_data->uc_chan_func = chan_info->channel_function;
    if (chan_info->channel_function == WS_CHAN_FUNC_FIXED) {
        fhss_data->uc_chan_fixed = chan_info->function.zero.fixed_channel;
        fhss_data->uc_chan_count = 1;
    } else {
        ws_neigh_set_chan_list(fhss_config, fhss_data->uc_channel_list, chan_info,
                               &fhss_data->uc_chan_count);
    }
    return offset_adjusted;
}

bool ws_neigh_duplicate_packet_check(struct ws_neigh *neigh, uint8_t mac_dsn, uint64_t rx_timestamp)
{
    if (neigh->last_dsn != mac_dsn) {
        // New packet always accepted
        neigh->last_dsn = mac_dsn;
        return true;
    }

    if (!neigh->unicast_data_rx) {
        // No unicast info stored always accepted
        return true;
    }

    rx_timestamp -= neigh->fhss_data.ffn.utt_rx_tstamp_us;
    rx_timestamp /= 1000000; //Convert to s

    //Compare only when last rx timestamp is less than 5 seconds
    if (rx_timestamp < 5) {
        //Packet is sent too fast filter it out
        return false;
    }

    return true;
}

int ws_neigh_lfn_count(struct ws_neigh_table *table)
{
    struct ws_neigh *neigh;
    int cnt = 0;

    SLIST_FOREACH(neigh, &table->neigh_list, link)
        if (neigh->node_role == WS_NR_ROLE_LFN)
            cnt++;
    return cnt;
}

void ws_neigh_trust(struct ws_neigh *neigh)
{
    if (neigh->trusted_device)
        return;

    neigh->expiration_s = time_current(CLOCK_MONOTONIC) + neigh->lifetime_s;
    neigh->trusted_device = true;
    TRACE(TR_NEIGH_15_4, "15.4 neighbor trusted %s / %ds", tr_eui64(neigh->mac64), neigh->lifetime_s);
}

void ws_neigh_refresh(struct ws_neigh *neigh, uint32_t lifetime_s)
{
    neigh->lifetime_s = lifetime_s;
    neigh->expiration_s = time_current(CLOCK_MONOTONIC) + lifetime_s;
    TRACE(TR_NEIGH_15_4, "15.4 neighbor refresh %s / %ds", tr_eui64(neigh->mac64), neigh->lifetime_s);
}
