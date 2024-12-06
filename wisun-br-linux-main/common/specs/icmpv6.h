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
 *
 */
#ifndef SPECS_ICMPV6_H
#define SPECS_ICMPV6_H

// RFC4443: "ICMPv6 Types".
// https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#icmpv6-parameters-2
enum {
    // 0 is reserved
    ICMPV6_TYPE_ERROR_DESTINATION_UNREACH            = 1,
    ICMPV6_TYPE_ERROR_PACKET_TOO_BIG                 = 2,
    ICMPV6_TYPE_ERROR_TIME_EXCEEDED                  = 3,
    ICMPV6_TYPE_ERROR_PARAMETER_PROBLEM              = 4,
    // 5-126 is unassigned or for private experimentation
    // 127 is reserved
    ICMPV6_TYPE_ECHO_REQUEST                         = 128,
    ICMPV6_TYPE_ECHO_REPLY                           = 129,
    ICMPV6_TYPE_MCAST_LIST_QUERY                     = 130,
    ICMPV6_TYPE_MCAST_LIST_REPORT                    = 131,
    ICMPV6_TYPE_MCAST_LIST_DONE                      = 132,
    ICMPV6_TYPE_RS                                   = 133,
    ICMPV6_TYPE_RA                                   = 134,
    ICMPV6_TYPE_NS                                   = 135,
    ICMPV6_TYPE_NA                                   = 136,
    ICMPV6_TYPE_REDIRECT                             = 137,
    ICMPV6_TYPE_ROUTER_RENUMBERING                   = 138,
    ICMPV6_TYPE_NODE_INFORMATION_QUERY               = 139,
    ICMPV6_TYPE_NODE_INFORMATION_RESPONSE            = 140,
    ICMPV6_TYPE_INVERSE_ND_SOLICITATION              = 141,
    ICMPV6_TYPE_INVERSE_ND_ADVERTISEMENT             = 142,
    ICMPV6_TYPE_MCAST_LIST_REPORT_V2                 = 143,
    ICMPV6_TYPE_HOME_AGENT_ADDRESS_DISCOVERY_REQUEST = 144,
    ICMPV6_TYPE_HOME_AGENT_ADDRESS_DISCOVERY_REPLY   = 145,
    ICMPV6_TYPE_MOBILE_PREFIX_SOLICITATION           = 146,
    ICMPV6_TYPE_MOBILE_PREFIX_ADVERTISEMENT          = 147,
    ICMPV6_TYPE_CERTIFICATION_PATH_SOLICITATION      = 148,
    ICMPV6_TYPE_CERTIFICATION_PATH_ADVERTISEMENT     = 149,
    // 150 is for experimental
    ICMPV6_TYPE_MCAST_ROUTER_ADVERTISEMENT           = 151,
    ICMPV6_TYPE_MCAST_ROUTER_SOLICITATION            = 152,
    ICMPV6_TYPE_MCAST_ROUTER_TERMINATION             = 153,
    ICMPV6_TYPE_FMIPV6                               = 154,
    ICMPV6_TYPE_RPL                                  = 155,
    ICMPV6_TYPE_ILNPV6_LOCATOR_UPDATE                = 156,
    ICMPV6_TYPE_DAR                                  = 157,
    ICMPV6_TYPE_DAC                                  = 158,
    ICMPV6_TYPE_MPL                                  = 159,
};

// RFC 4443: Error codes for "Destination Unreachable".
// https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#icmpv6-parameters-codes-2
enum {
    ICMPV6_CODE_DST_UNREACH_NO_ROUTE             = 0,
    ICMPV6_CODE_DST_UNREACH_ADM_PROHIB           = 1,
    ICMPV6_CODE_DST_UNREACH_BEYOND_SCOPE         = 2,
    ICMPV6_CODE_DST_UNREACH_ADDR_UNREACH         = 3,
    ICMPV6_CODE_DST_UNREACH_PORT_UNREACH         = 4,
    ICMPV6_CODE_DST_UNREACH_SRC_FAILED_POLICY    = 5,
    ICMPV6_CODE_DST_UNREACH_ROUTE_REJECTED       = 6,
    ICMPV6_CODE_DST_UNREACH_SRH                  = 7,
};

// RFC 4443: Error codes for "Time Exceeded".
// https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#icmpv6-parameters-codes-4
enum {
    ICMPV6_CODE_TME_EXCD_HOP_LIM_EXCD          = 0,
    ICMPV6_CODE_TME_EXCD_FRG_REASS_TME_EXCD    = 1,
};

// RFC 4443: Error codes for "Parameter Problem".
// https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#icmpv6-parameters-codes-5
enum {
    ICMPV6_CODE_PARAM_PRB_HDR_ERR              = 0,
    ICMPV6_CODE_PARAM_PRB_UNREC_NEXT_HDR       = 1,
    ICMPV6_CODE_PARAM_PRB_UNREC_IPV6_OPT       = 2,
    ICMPV6_CODE_PARAM_PRB_FIRST_FRAG_IPV6_HDR  = 3,
};

// RFC 4861: "IPv6 Neighbor Discovery Option Formats".
// https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#icmpv6-parameters-5
enum {
    ICMPV6_OPT_SRC_LL_ADDR                   = 1,
    ICMPV6_OPT_TGT_LL_ADDR                   = 2,
    // ...
    ICMPV6_OPT_ADDR_REGISTRATION             = 33,
};

// RFC6775: "Address Registration Option Status Values"
// https://www.iana.org/assignments/icmpv6-parameters/icmpv6-parameters.xhtml#address-registration
enum {
    ARO_SUCCESS                 = 0,
    ARO_DUPLICATE               = 1,
    ARO_FULL                    = 2,
    ARO_MOVED                   = 3,
    ARO_REMOVED                 = 4,
    ARO_VALIDATION_REQUESTED    = 5,
    ARO_DUPLICATE_SOURCE_ADDR   = 6,
    ARO_INVALID_SOURCE_ADDR     = 7,
    ARO_TOPOLOGICALLY_INCORRECT = 8,
};
#endif
