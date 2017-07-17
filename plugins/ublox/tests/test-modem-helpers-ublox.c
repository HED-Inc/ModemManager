/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2016 Aleksander Morgado <aleksander@aleksander.es>
 */

#include <glib.h>
#include <glib-object.h>
#include <locale.h>
#include <arpa/inet.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-log.h"
#include "mm-modem-helpers.h"
#include "mm-modem-helpers-ublox.h"

/*****************************************************************************/
/* Test +UPINCNT responses */

typedef struct {
    const gchar *str;
    guint        pin_attempts;
    guint        pin2_attempts;
    guint        puk_attempts;
    guint        puk2_attempts;
} UpinCntResponseTest;

static const UpinCntResponseTest upincnt_response_tests[] = {
    { .str = "+UPINCNT: 3,3,10,10\r\n",
      .pin_attempts  = 3,
      .pin2_attempts = 3,
      .puk_attempts  = 10,
      .puk2_attempts = 10
    },
    { .str = "+UPINCNT: 0,3,5,5\r\n",
      .pin_attempts  = 0,
      .pin2_attempts = 3,
      .puk_attempts  = 5,
      .puk2_attempts = 5
    },
    { .str = "+UPINCNT: 0,0,0,0\r\n",
      .pin_attempts  = 0,
      .pin2_attempts = 0,
      .puk_attempts  = 0,
      .puk2_attempts = 0
    },
};

static void
test_upincnt_response (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (upincnt_response_tests); i++) {
        GError   *error = NULL;
        gboolean  success;
        guint     pin_attempts  = G_MAXUINT;
        guint     pin2_attempts = G_MAXUINT;
        guint     puk_attempts  = G_MAXUINT;
        guint     puk2_attempts = G_MAXUINT;

        success = mm_ublox_parse_upincnt_response (upincnt_response_tests[i].str,
                                                   &pin_attempts, &pin2_attempts,
                                                   &puk_attempts, &puk2_attempts,
                                                   &error);
        g_assert_no_error (error);
        g_assert (success);
        g_assert_cmpuint (upincnt_response_tests[i].pin_attempts,  ==, pin_attempts);
        g_assert_cmpuint (upincnt_response_tests[i].pin2_attempts, ==, pin2_attempts);
        g_assert_cmpuint (upincnt_response_tests[i].puk_attempts,  ==, puk_attempts);
        g_assert_cmpuint (upincnt_response_tests[i].puk2_attempts, ==, puk2_attempts);
    }
}

/*****************************************************************************/
/* Test UUSBCONF? responses */

typedef struct {
    const gchar       *str;
    MMUbloxUsbProfile  profile;
} UusbconfResponseTest;

static const UusbconfResponseTest uusbconf_response_tests[] = {
    {
        .str     = "+UUSBCONF: 3,\"RNDIS\",,\"0x1146\"\r\n",
        .profile = MM_UBLOX_USB_PROFILE_RNDIS
    },
    {
        .str     = "+UUSBCONF: 2,\"ECM\",,\"0x1143\"\r\n",
        .profile = MM_UBLOX_USB_PROFILE_ECM
    },
    {
        .str     = "+UUSBCONF: 0,\"\",,\"0x1141\"\r\n",
        .profile = MM_UBLOX_USB_PROFILE_BACK_COMPATIBLE
    },
};

static void
test_uusbconf_response (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (uusbconf_response_tests); i++) {
        MMUbloxUsbProfile profile = MM_UBLOX_USB_PROFILE_UNKNOWN;
        GError *error = NULL;
        gboolean success;

        success = mm_ublox_parse_uusbconf_response (uusbconf_response_tests[i].str, &profile, &error);
        g_assert_no_error (error);
        g_assert (success);
        g_assert_cmpuint (uusbconf_response_tests[i].profile, ==, profile);
    }
}

/*****************************************************************************/
/* Test UBMCONF? responses */

typedef struct {
    const gchar           *str;
    MMUbloxNetworkingMode  mode;
} UbmconfResponseTest;

static const UbmconfResponseTest ubmconf_response_tests[] = {
    {
        .str  = "+UBMCONF: 1\r\n",
        .mode = MM_UBLOX_NETWORKING_MODE_ROUTER
    },
    {
        .str  = "+UBMCONF: 2\r\n",
        .mode = MM_UBLOX_NETWORKING_MODE_BRIDGE
    },
};

static void
test_ubmconf_response (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (ubmconf_response_tests); i++) {
        MMUbloxNetworkingMode mode = MM_UBLOX_NETWORKING_MODE_UNKNOWN;
        GError *error = NULL;
        gboolean success;

        success = mm_ublox_parse_ubmconf_response (ubmconf_response_tests[i].str, &mode, &error);
        g_assert_no_error (error);
        g_assert (success);
        g_assert_cmpuint (ubmconf_response_tests[i].mode, ==, mode);
    }
}

/*****************************************************************************/
/* Test UIPADDR=N responses */

typedef struct {
    const gchar *str;
    guint        cid;
    const gchar *if_name;
    const gchar *ipv4_address;
    const gchar *ipv4_subnet;
    const gchar *ipv6_global_address;
    const gchar *ipv6_link_local_address;
} UipaddrResponseTest;

static const UipaddrResponseTest uipaddr_response_tests[] = {
    {
        .str = "+UIPADDR: 1,\"ccinet0\",\"5.168.120.13\",\"255.255.255.0\",\"\",\"\"",
        .cid = 1,
        .if_name = "ccinet0",
        .ipv4_address = "5.168.120.13",
        .ipv4_subnet = "255.255.255.0",
    },
    {
        .str = "+UIPADDR: 2,\"ccinet1\",\"\",\"\",\"2001::1:200:FF:FE00:0/64\",\"FE80::200:FF:FE00:0/64\"",
        .cid = 2,
        .if_name = "ccinet1",
        .ipv6_global_address = "2001::1:200:FF:FE00:0/64",
        .ipv6_link_local_address = "FE80::200:FF:FE00:0/64",
    },
    {
        .str = "+UIPADDR: 3,\"ccinet2\",\"5.10.100.2\",\"255.255.255.0\",\"2001::1:200:FF:FE00:0/64\",\"FE80::200:FF:FE00:0/64\"",
        .cid = 3,
        .if_name = "ccinet2",
        .ipv4_address = "5.10.100.2",
        .ipv4_subnet = "255.255.255.0",
        .ipv6_global_address = "2001::1:200:FF:FE00:0/64",
        .ipv6_link_local_address = "FE80::200:FF:FE00:0/64",
    },
};

static void
test_uipaddr_response (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (uipaddr_response_tests); i++) {
        GError   *error = NULL;
        gboolean  success;
        guint     cid = G_MAXUINT;
        gchar    *if_name = NULL;
        gchar    *ipv4_address = NULL;
        gchar    *ipv4_subnet = NULL;
        gchar    *ipv6_global_address = NULL;
        gchar    *ipv6_link_local_address = NULL;

        success = mm_ublox_parse_uipaddr_response (uipaddr_response_tests[i].str,
                                                   &cid,
                                                   &if_name,
                                                   &ipv4_address,
                                                   &ipv4_subnet,
                                                   &ipv6_global_address,
                                                   &ipv6_link_local_address,
                                                   &error);
        g_assert_no_error (error);
        g_assert (success);
        g_assert_cmpuint (uipaddr_response_tests[i].cid,                     ==, cid);
        g_assert_cmpstr  (uipaddr_response_tests[i].if_name,                 ==, if_name);
        g_assert_cmpstr  (uipaddr_response_tests[i].ipv4_address,            ==, ipv4_address);
        g_assert_cmpstr  (uipaddr_response_tests[i].ipv4_subnet,             ==, ipv4_subnet);
        g_assert_cmpstr  (uipaddr_response_tests[i].ipv6_global_address,     ==, ipv6_global_address);
        g_assert_cmpstr  (uipaddr_response_tests[i].ipv6_link_local_address, ==, ipv6_link_local_address);

        g_free (if_name);
        g_free (ipv4_address);
        g_free (ipv4_subnet);
        g_free (ipv6_global_address);
        g_free (ipv6_link_local_address);
    }
}

/*****************************************************************************/
/* Test CFUN? response */

typedef struct {
    const gchar       *str;
    MMModemPowerState  state;
} CfunQueryTest;

static const CfunQueryTest cfun_query_tests[] = {
    { "+CFUN: 1",    MM_MODEM_POWER_STATE_ON  },
    { "+CFUN: 1,0",  MM_MODEM_POWER_STATE_ON  },
    { "+CFUN: 0",    MM_MODEM_POWER_STATE_LOW },
    { "+CFUN: 0,0",  MM_MODEM_POWER_STATE_LOW },
    { "+CFUN: 4",    MM_MODEM_POWER_STATE_LOW },
    { "+CFUN: 4,0",  MM_MODEM_POWER_STATE_LOW },
    { "+CFUN: 19",   MM_MODEM_POWER_STATE_LOW },
    { "+CFUN: 19,0", MM_MODEM_POWER_STATE_LOW },
};

static void
test_cfun_response (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (cfun_query_tests); i++) {
        GError            *error = NULL;
        gboolean           success;
        MMModemPowerState  state = MM_MODEM_POWER_STATE_UNKNOWN;

        success = mm_ublox_parse_cfun_response (cfun_query_tests[i].str, &state, &error);
        g_assert_no_error (error);
        g_assert (success);
        g_assert_cmpuint (cfun_query_tests[i].state, ==, state);
    }
}

/*****************************************************************************/
/* Test URAT=? responses and model based filtering */

static void
compare_combinations (const gchar                  *response,
                      const gchar                  *model,
                      const MMModemModeCombination *expected_combinations,
                      guint                         n_expected_combinations)
{
    GArray *combinations;
    GError *error = NULL;
    guint i;

    combinations = mm_ublox_parse_urat_test_response (response, &error);
    g_assert_no_error (error);
    g_assert (combinations);

    combinations = mm_ublox_filter_supported_modes (model, combinations, &error);
    g_assert_no_error (error);
    g_assert (combinations);

    g_assert_cmpuint (combinations->len, ==, n_expected_combinations);

    for (i = 0; i < combinations->len; i++) {
        MMModemModeCombination combination;
        guint                  j;
        gboolean               found = FALSE;

        combination = g_array_index (combinations, MMModemModeCombination, i);
        for (j = 0; !found && j < n_expected_combinations; j++)
            found = (combination.allowed == expected_combinations[j].allowed &&
                     combination.preferred == expected_combinations[j].preferred);
        g_assert (found);
    }

    g_array_unref (combinations);
}

static void
test_urat_test_response_2g (void)
{
    static const MMModemModeCombination expected_combinations[] = {
        { MM_MODEM_MODE_2G, MM_MODEM_MODE_NONE }
    };

    compare_combinations ("+URAT: 0",       NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
    compare_combinations ("+URAT: 0,0",     NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
    compare_combinations ("+URAT: (0)",     NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
    compare_combinations ("+URAT: (0),(0)", NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
}

static void
test_urat_test_response_2g3g (void)
{
    static const MMModemModeCombination expected_combinations[] = {
        { MM_MODEM_MODE_2G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_2G },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_3G },
    };

    compare_combinations ("+URAT: (0,1,2),(0,2)", NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
    compare_combinations ("+URAT: (0-2),(0,2)",   NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
}

static void
test_urat_test_response_2g3g4g (void)
{
    static const MMModemModeCombination expected_combinations[] = {
        { MM_MODEM_MODE_2G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE },

        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_2G },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_3G },

        { MM_MODEM_MODE_2G | MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_4G, MM_MODEM_MODE_2G },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_4G, MM_MODEM_MODE_4G },

        { MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_3G },
        { MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_4G },

        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_2G },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_3G },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_4G },
    };

    compare_combinations ("+URAT: (0,1,2,3,4,5,6),(0,2,3)", NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
    compare_combinations ("+URAT: (0-6),(0,2,3)",           NULL, expected_combinations, G_N_ELEMENTS (expected_combinations));
}

static void
test_mode_filtering_toby_l201 (void)
{
    static const MMModemModeCombination expected_combinations[] = {
        { MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE },

        { MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_3G },
        { MM_MODEM_MODE_3G | MM_MODEM_MODE_4G, MM_MODEM_MODE_4G },
    };

    compare_combinations ("+URAT: (0-6),(0,2,3)", "TOBY-L201", expected_combinations, G_N_ELEMENTS (expected_combinations));
}

static void
test_mode_filtering_lisa_u200 (void)
{
    static const MMModemModeCombination expected_combinations[] = {
        { MM_MODEM_MODE_2G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },

        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_2G },
        { MM_MODEM_MODE_2G | MM_MODEM_MODE_3G, MM_MODEM_MODE_3G },
    };

    compare_combinations ("+URAT: (0-6),(0,2,3)", "LISA-U200", expected_combinations, G_N_ELEMENTS (expected_combinations));
}

static void
test_mode_filtering_sara_u280 (void)
{
    static const MMModemModeCombination expected_combinations[] = {
        { MM_MODEM_MODE_3G, MM_MODEM_MODE_NONE },
    };

    compare_combinations ("+URAT: (0-6),(0,2,3)", "SARA-U280", expected_combinations, G_N_ELEMENTS (expected_combinations));
}

/*****************************************************************************/
/* URAT? response parser and URAT=X command builder */

typedef struct {
    const gchar *command;
    const gchar *response;
    MMModemMode  allowed;
    MMModemMode  preferred;
} UratTest;

static const UratTest urat_tests[] = {
    {
        .command   = "+URAT=1,2",
        .response  = "+URAT: 1,2\r\n",
        .allowed   = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G),
        .preferred = MM_MODEM_MODE_3G,
    },
    {
        .command   = "+URAT=4,0",
        .response  = "+URAT: 4,0\r\n",
        .allowed   = (MM_MODEM_MODE_2G | MM_MODEM_MODE_3G | MM_MODEM_MODE_4G),
        .preferred = MM_MODEM_MODE_2G,
    },
    {
        .command   = "+URAT=0",
        .response  = "+URAT: 0\r\n",
        .allowed   = MM_MODEM_MODE_2G,
        .preferred = MM_MODEM_MODE_NONE,
    },
    {
        .command   = "+URAT=6",
        .response  = "+URAT: 6\r\n",
        .allowed   = (MM_MODEM_MODE_3G | MM_MODEM_MODE_4G),
        .preferred = MM_MODEM_MODE_NONE,
    },
};

static void
test_urat_read_response (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (urat_tests); i++) {
        MMModemMode  allowed   = MM_MODEM_MODE_NONE;
        MMModemMode  preferred = MM_MODEM_MODE_NONE;
        GError      *error = NULL;
        gboolean     success;

        success = mm_ublox_parse_urat_read_response (urat_tests[i].response,
                                                     &allowed, &preferred, &error);
        g_assert_no_error (error);
        g_assert (success);
        g_assert_cmpuint (urat_tests[i].allowed,   ==, allowed);
        g_assert_cmpuint (urat_tests[i].preferred, ==, preferred);
    }
}

static void
test_urat_write_command (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (urat_tests); i++) {
        gchar  *command;
        GError *error = NULL;

        command = mm_ublox_build_urat_set_command (urat_tests[i].allowed, urat_tests[i].preferred, &error);
        g_assert_no_error (error);
        g_assert_cmpstr (command, ==, urat_tests[i].command);
        g_free (command);
    }
}

/*****************************************************************************/
/* Supported bands */

static void
common_validate_supported_bands (const gchar       *model,
                                 const MMModemBand *expected_bands,
                                 guint              n_expected_bands)
{
    GError *error = NULL;
    GArray *bands;
    gchar  *bands_str;
    GArray *expected_bands_array;
    gchar  *expected_bands_str;

    bands = mm_ublox_get_supported_bands (model, &error);
    g_assert_no_error (error);
    g_assert (bands);
    mm_common_bands_garray_sort (bands);
    bands_str = mm_common_build_bands_string ((MMModemBand *)(bands->data), bands->len);
    g_array_unref (bands);

    expected_bands_array = g_array_sized_new (FALSE, FALSE, sizeof (MMModemBand), n_expected_bands);
    g_array_append_vals (expected_bands_array, expected_bands, n_expected_bands);
    mm_common_bands_garray_sort (expected_bands_array);
    expected_bands_str = mm_common_build_bands_string ((MMModemBand *)(expected_bands_array->data), expected_bands_array->len);
    g_array_unref (expected_bands_array);

    g_assert_cmpstr (bands_str, ==, expected_bands_str);
    g_free (bands_str);
    g_free (expected_bands_str);
}

static void
test_supported_bands_all (void)
{
    /* All 2G, 3G and 4G bands */
    const MMModemBand expected_bands[] = {
        /*  700 */ MM_MODEM_BAND_EUTRAN_13, MM_MODEM_BAND_EUTRAN_17,
        /*  800 */ MM_MODEM_BAND_U800, MM_MODEM_BAND_EUTRAN_20,
        /*  850 */ MM_MODEM_BAND_G850, MM_MODEM_BAND_U850, MM_MODEM_BAND_EUTRAN_5,
        /*  900 */ MM_MODEM_BAND_EGSM, MM_MODEM_BAND_U900, MM_MODEM_BAND_EUTRAN_8,
        /* 1500 */ MM_MODEM_BAND_EUTRAN_11,
        /* 1700 */ MM_MODEM_BAND_U17IV, MM_MODEM_BAND_EUTRAN_4,
        /* 1800 */ MM_MODEM_BAND_DCS, MM_MODEM_BAND_U1800, MM_MODEM_BAND_EUTRAN_3,
        /* 1900 */ MM_MODEM_BAND_PCS, MM_MODEM_BAND_U1900, MM_MODEM_BAND_EUTRAN_2,
        /* 2100 */ MM_MODEM_BAND_U2100, MM_MODEM_BAND_EUTRAN_1,
        /* 2600 */ MM_MODEM_BAND_U2600, MM_MODEM_BAND_EUTRAN_7,
    };

    common_validate_supported_bands (NULL, expected_bands, G_N_ELEMENTS (expected_bands));
}

static void
test_supported_bands_toby_l201 (void)
{
    /* Only 3G and 4G bands */
    const MMModemBand expected_bands[] = {
        /*  700 */ MM_MODEM_BAND_EUTRAN_13, MM_MODEM_BAND_EUTRAN_17,
        /*  800 */ MM_MODEM_BAND_U800, MM_MODEM_BAND_EUTRAN_20,
        /*  850 */ MM_MODEM_BAND_U850, MM_MODEM_BAND_EUTRAN_5,
        /*  900 */ MM_MODEM_BAND_U900, MM_MODEM_BAND_EUTRAN_8,
        /* 1500 */ MM_MODEM_BAND_EUTRAN_11,
        /* 1700 */ MM_MODEM_BAND_U17IV, MM_MODEM_BAND_EUTRAN_4,
        /* 1800 */ MM_MODEM_BAND_U1800, MM_MODEM_BAND_EUTRAN_3,
        /* 1900 */ MM_MODEM_BAND_U1900, MM_MODEM_BAND_EUTRAN_2,
        /* 2100 */ MM_MODEM_BAND_U2100, MM_MODEM_BAND_EUTRAN_1,
        /* 2600 */ MM_MODEM_BAND_U2600, MM_MODEM_BAND_EUTRAN_7,
    };

    common_validate_supported_bands ("TOBY-L201", expected_bands, G_N_ELEMENTS (expected_bands));
}

static void
test_supported_bands_lisa_u200 (void)
{
    /* Only 2G and 3G bands */
    const MMModemBand expected_bands[] = {
        /*  800 */ MM_MODEM_BAND_U800,
        /*  850 */ MM_MODEM_BAND_G850, MM_MODEM_BAND_U850,
        /*  900 */ MM_MODEM_BAND_EGSM, MM_MODEM_BAND_U900,
        /* 1700 */ MM_MODEM_BAND_U17IV,
        /* 1800 */ MM_MODEM_BAND_DCS, MM_MODEM_BAND_U1800,
        /* 1900 */ MM_MODEM_BAND_PCS, MM_MODEM_BAND_U1900,
        /* 2100 */ MM_MODEM_BAND_U2100,
        /* 2600 */ MM_MODEM_BAND_U2600,
    };

    common_validate_supported_bands ("LISA-U200", expected_bands, G_N_ELEMENTS (expected_bands));
}

static void
test_supported_bands_sara_u280 (void)
{
    /* Only 3G bands */
    const MMModemBand expected_bands[] = {
        /*  800 */ MM_MODEM_BAND_U800,
        /*  850 */ MM_MODEM_BAND_U850,
        /*  900 */ MM_MODEM_BAND_U900,
        /* 1700 */ MM_MODEM_BAND_U17IV,
        /* 1800 */ MM_MODEM_BAND_U1800,
        /* 1900 */ MM_MODEM_BAND_U1900,
        /* 2100 */ MM_MODEM_BAND_U2100,
        /* 2600 */ MM_MODEM_BAND_U2600,
    };

    common_validate_supported_bands ("SARA-U280", expected_bands, G_N_ELEMENTS (expected_bands));
}

/*****************************************************************************/
/* Test +UBANDSEL? response parser */

static void
common_validate_ubandsel_response (const gchar       *str,
                                   const MMModemBand *expected_bands,
                                   guint              n_expected_bands)
{
    GError *error = NULL;
    GArray *bands;
    gchar  *bands_str;
    GArray *expected_bands_array;
    gchar  *expected_bands_str;

    bands = mm_ublox_parse_ubandsel_response (str, &error);
    g_assert_no_error (error);
    g_assert (bands);
    mm_common_bands_garray_sort (bands);
    bands_str = mm_common_build_bands_string ((MMModemBand *)(bands->data), bands->len);
    g_array_unref (bands);

    expected_bands_array = g_array_sized_new (FALSE, FALSE, sizeof (MMModemBand), n_expected_bands);
    g_array_append_vals (expected_bands_array, expected_bands, n_expected_bands);
    mm_common_bands_garray_sort (expected_bands_array);
    expected_bands_str = mm_common_build_bands_string ((MMModemBand *)(expected_bands_array->data), expected_bands_array->len);
    g_array_unref (expected_bands_array);

    g_assert_cmpstr (bands_str, ==, expected_bands_str);
    g_free (bands_str);
    g_free (expected_bands_str);
}

static void
test_ubandsel_response_four (void)
{
    const MMModemBand expected_bands[] = {
        /*  850 */ MM_MODEM_BAND_G850, MM_MODEM_BAND_U850, MM_MODEM_BAND_EUTRAN_5,
        /*  900 */ MM_MODEM_BAND_EGSM, MM_MODEM_BAND_U900, MM_MODEM_BAND_EUTRAN_8,
        /* 1800 */ MM_MODEM_BAND_DCS, MM_MODEM_BAND_U1800, MM_MODEM_BAND_EUTRAN_3,
        /* 1900 */ MM_MODEM_BAND_PCS, MM_MODEM_BAND_U1900, MM_MODEM_BAND_EUTRAN_2,
    };

    common_validate_ubandsel_response ("+UBANDSEL: 850,900,1800,1900\r\n", expected_bands, G_N_ELEMENTS (expected_bands));
}

static void
test_ubandsel_response_three (void)
{
    const MMModemBand expected_bands[] = {
        /*  850 */ MM_MODEM_BAND_G850, MM_MODEM_BAND_U850, MM_MODEM_BAND_EUTRAN_5,
        /*  900 */ MM_MODEM_BAND_EGSM, MM_MODEM_BAND_U900, MM_MODEM_BAND_EUTRAN_8,
        /* 1800 */ MM_MODEM_BAND_DCS, MM_MODEM_BAND_U1800, MM_MODEM_BAND_EUTRAN_3,
    };

    common_validate_ubandsel_response ("+UBANDSEL: 850,900,1800\r\n", expected_bands, G_N_ELEMENTS (expected_bands));
}

static void
test_ubandsel_response_two (void)
{
    const MMModemBand expected_bands[] = {
        /*  850 */ MM_MODEM_BAND_G850, MM_MODEM_BAND_U850, MM_MODEM_BAND_EUTRAN_5,
        /*  900 */ MM_MODEM_BAND_EGSM, MM_MODEM_BAND_U900, MM_MODEM_BAND_EUTRAN_8,
    };

    common_validate_ubandsel_response ("+UBANDSEL: 850,900\r\n", expected_bands, G_N_ELEMENTS (expected_bands));
}

static void
test_ubandsel_response_one (void)
{
    const MMModemBand expected_bands[] = {
        /*  850 */ MM_MODEM_BAND_G850, MM_MODEM_BAND_U850, MM_MODEM_BAND_EUTRAN_5,
    };

    common_validate_ubandsel_response ("+UBANDSEL: 850\r\n", expected_bands, G_N_ELEMENTS (expected_bands));
}

/*****************************************************************************/
/* +UBANDSEL=x command builder */

static void
common_validate_ubandsel_request (const MMModemBand *bands,
                                  guint              n_bands,
                                  const gchar       *expected_request)
{
    GError *error = NULL;
    GArray *bands_array;
    gchar  *request;

    bands_array = g_array_sized_new (FALSE, FALSE, sizeof (MMModemBand), n_bands);
    g_array_append_vals (bands_array, bands, n_bands);

    request = mm_ublox_build_ubandsel_set_command (bands_array, &error);
    g_assert_no_error (error);
    g_assert (request);

    g_assert_cmpstr (request, ==, expected_request);

    g_array_unref (bands_array);
    g_free (request);
}

static void
test_ubandsel_request_any (void)
{
    const MMModemBand bands[] = {
        MM_MODEM_BAND_ANY
    };

    common_validate_ubandsel_request (bands, G_N_ELEMENTS (bands), "+UBANDSEL=0");
}

static void
test_ubandsel_request_2g (void)
{
    const MMModemBand bands[] = {
        MM_MODEM_BAND_G850, MM_MODEM_BAND_EGSM, MM_MODEM_BAND_DCS, MM_MODEM_BAND_PCS
    };

    common_validate_ubandsel_request (bands, G_N_ELEMENTS (bands), "+UBANDSEL=850,900,1800,1900");
}

static void
test_ubandsel_request_1800 (void)
{
    const MMModemBand bands[] = {
        MM_MODEM_BAND_DCS, MM_MODEM_BAND_U1800, MM_MODEM_BAND_EUTRAN_3
    };

    common_validate_ubandsel_request (bands, G_N_ELEMENTS (bands), "+UBANDSEL=1800");
}

/*****************************************************************************/
/* Test +UGCNTRD responses */

typedef struct {
    const gchar *str;
    guint        cid;
    guint        session_tx_bytes;
    guint        session_rx_bytes;
    guint        total_tx_bytes;
    guint        total_rx_bytes;
} UgcntrdResponseTest;

static const UgcntrdResponseTest ugcntrd_response_tests[] = {
    {
        .str = "+UGCNTRD: 1, 100, 0, 100, 0",
        .cid              = 1,
        .session_tx_bytes = 100,
        .session_rx_bytes = 0,
        .total_tx_bytes   = 100,
        .total_rx_bytes   = 0
    },
    {
        .str = "+UGCNTRD: 31,2704,1819,2724,1839",
        .cid              = 31,
        .session_tx_bytes = 2704,
        .session_rx_bytes = 1819,
        .total_tx_bytes   = 2724,
        .total_rx_bytes   = 1839
    },
    {
        .str = "+UGCNTRD: 1, 100, 0, 100, 0\r\n"
               "+UGCNTRD: 31,2704,1819,2724,1839\r\n",
        .cid              = 1,
        .session_tx_bytes = 100,
        .session_rx_bytes = 0,
        .total_tx_bytes   = 100,
        .total_rx_bytes   = 0
    },
    {
        .str = "+UGCNTRD: 1, 100, 0, 100, 0\r\n"
               "+UGCNTRD: 31,2704,1819,2724,1839\r\n",
        .cid              = 31,
        .session_tx_bytes = 2704,
        .session_rx_bytes = 1819,
        .total_tx_bytes   = 2724,
        .total_rx_bytes   = 1839
    }
};

static void
test_ugcntrd_response (void)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (ugcntrd_response_tests); i++) {
        GError   *error = NULL;
        gboolean  success;
        guint     session_tx_bytes = G_MAXUINT;
        guint     session_rx_bytes = G_MAXUINT;
        guint     total_tx_bytes   = G_MAXUINT;
        guint     total_rx_bytes   = G_MAXUINT;

        success = mm_ublox_parse_ugcntrd_response_for_cid (ugcntrd_response_tests[i].str,
                                                           ugcntrd_response_tests[i].cid,
                                                           &session_tx_bytes,
                                                           &session_rx_bytes,
                                                           &total_tx_bytes,
                                                           &total_rx_bytes,
                                                           &error);
        g_assert_no_error (error);
        g_assert (success);
        g_assert_cmpuint (ugcntrd_response_tests[i].session_tx_bytes, ==, session_tx_bytes);
        g_assert_cmpuint (ugcntrd_response_tests[i].session_rx_bytes, ==, session_rx_bytes);
        g_assert_cmpuint (ugcntrd_response_tests[i].total_tx_bytes,   ==, total_tx_bytes);
        g_assert_cmpuint (ugcntrd_response_tests[i].total_rx_bytes,   ==, total_rx_bytes);
    }
}

/*****************************************************************************/

void
_mm_log (const char *loc,
         const char *func,
         guint32 level,
         const char *fmt,
         ...)
{
#if defined ENABLE_TEST_MESSAGE_TRACES
    /* Dummy log function */
    va_list args;
    gchar *msg;

    va_start (args, fmt);
    msg = g_strdup_vprintf (fmt, args);
    va_end (args);
    g_print ("%s\n", msg);
    g_free (msg);
#endif
}

int main (int argc, char **argv)
{
    setlocale (LC_ALL, "");

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/MM/ublox/upincnt/response",  test_upincnt_response);
    g_test_add_func ("/MM/ublox/uusbconf/response", test_uusbconf_response);
    g_test_add_func ("/MM/ublox/ubmconf/response",  test_ubmconf_response);
    g_test_add_func ("/MM/ublox/uipaddr/response",  test_uipaddr_response);
    g_test_add_func ("/MM/ublox/cfun/response",     test_cfun_response);
    g_test_add_func ("/MM/ublox/urat/test/response/2g",        test_urat_test_response_2g);
    g_test_add_func ("/MM/ublox/urat/test/response/2g3g",      test_urat_test_response_2g3g);
    g_test_add_func ("/MM/ublox/urat/test/response/2g3g4g",    test_urat_test_response_2g3g4g);
    g_test_add_func ("/MM/ublox/urat/test/response/toby-l201", test_mode_filtering_toby_l201);
    g_test_add_func ("/MM/ublox/urat/test/response/lisa-u200", test_mode_filtering_lisa_u200);
    g_test_add_func ("/MM/ublox/urat/test/response/sara-u280", test_mode_filtering_sara_u280);
    g_test_add_func ("/MM/ublox/urat/read/response", test_urat_read_response);
    g_test_add_func ("/MM/ublox/urat/write/command", test_urat_write_command);
    g_test_add_func ("/MM/ublox/supported-bands/all",       test_supported_bands_all);
    g_test_add_func ("/MM/ublox/supported-bands/toby-l201", test_supported_bands_toby_l201);
    g_test_add_func ("/MM/ublox/supported-bands/lisa-u200", test_supported_bands_lisa_u200);
    g_test_add_func ("/MM/ublox/supported-bands/sara-u280", test_supported_bands_sara_u280);

    g_test_add_func ("/MM/ublox/ubandsel/response/one",   test_ubandsel_response_one);
    g_test_add_func ("/MM/ublox/ubandsel/response/two",   test_ubandsel_response_two);
    g_test_add_func ("/MM/ublox/ubandsel/response/three", test_ubandsel_response_three);
    g_test_add_func ("/MM/ublox/ubandsel/response/four",  test_ubandsel_response_four);

    g_test_add_func ("/MM/ublox/ubandsel/request/any",  test_ubandsel_request_any);
    g_test_add_func ("/MM/ublox/ubandsel/request/2g",   test_ubandsel_request_2g);
    g_test_add_func ("/MM/ublox/ubandsel/request/1800", test_ubandsel_request_1800);

    g_test_add_func ("/MM/ublox/ugcntrd/response", test_ugcntrd_response);

    return g_test_run ();
}