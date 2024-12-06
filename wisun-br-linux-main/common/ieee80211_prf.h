/*
 * Copyright (c) 2016-2018, Pelion and affiliates.
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
#ifndef IEEE80211_PRF_H
#define IEEE80211_PRF_H
#include <stdint.h>
#include <stddef.h>

/*
 * Pseudo-Random Function (PRF) producing n bits of output, described in IEEE
 * 802.11-2020 12.7.1.2. This is used to generated nonce and PTK in 802.11i.
 *
 * MbedTLS provide a really similar function: mbedtls_ssl_tls_prf(). However,
 * mbedtls_ssl_tls_prf() does not support SHA1 as hash function.
 */

int ieee80211_prf(const uint8_t *key, size_t key_len, const char *label,
                  const uint8_t *data, size_t data_len,
                  uint8_t *result, size_t result_size);

#endif
