/*
 * Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Tests for X509 time functions */

#include <string.h>
#include <time.h>

#include <openssl/asn1.h>
#include <openssl/x509.h>
#include "testutil.h"
#include "e_os.h"

typedef struct {
    const char *data;
    int type;
    time_t cmp_time;
    /* -1 if asn1_time <= cmp_time, 1 if asn1_time > cmp_time, 0 if error. */
    int expected;
} TESTDATA;

typedef struct {
    const char *data;
    /* 0 for check-only mode, 1 for set-string mode */
    int set_string;
    /* 0 for error, 1 if succeed */
    int expected;
    /*
     * The following 2 fields are ignored if set_string field is set to '0'
     * (in check only mode).
     *
     * But they can still be ignored explicitly in set-string mode by:
     * setting -1 to expected_type and setting NULL to expected_string.
     *
     * It's useful in a case of set-string mode but the expected result
     * is a 'parsing error'.
     */
    int expected_type;
    const char *expected_string;
} TESTDATA_FORMAT;

/*
 * Actually, the "loose" mode has been tested in
 * those time-compare-cases, so we may not test it again.
 */
static TESTDATA_FORMAT x509_format_tests[] = {
    /* GeneralizedTime */
    {
        /* good format, check only */
        "20170217180105Z", 0, 1, -1, NULL,
    },
    {
        /* SS is missing, check only */
        "201702171801Z", 0, 0, -1, NULL,
    },
    {
        /* fractional seconds, check only */
        "20170217180105.001Z", 0, 0, -1, NULL,
    },
    {
        /* time zone, check only */
        "20170217180105+0800", 0, 0, -1, NULL,
    },
    {
        /* SS is missing, set string */
        "201702171801Z", 1, 0, -1, NULL,
    },
    {
        /* fractional seconds, set string */
        "20170217180105.001Z", 1, 0, -1, NULL,
    },
    {
        /* time zone, set string */
        "20170217180105+0800", 1, 0, -1, NULL,
    },
    {
        /* good format, check returned 'turned' string */
        "20170217180154Z", 1, 1, V_ASN1_UTCTIME, "170217180154Z",
    },
    {
        /* good format, check returned string */
        "20510217180154Z", 1, 1, V_ASN1_GENERALIZEDTIME, "20510217180154Z",
    },
    {
        /* good format but out of UTC range, check returned string */
        "19230419180154Z", 1, 1, V_ASN1_GENERALIZEDTIME, "19230419180154Z",
    },
    /* UTC */
    {
        /* SS is missing, check only */
        "1702171801Z", 0, 0, -1, NULL,
    },
    {
        /* time zone, check only */
        "170217180154+0800", 0, 0, -1, NULL,
    },
    {
        /* SS is missing, set string */
        "1702171801Z", 1, 0, -1, NULL,
    },
    {
        /* time zone, set string */
        "170217180154+0800", 1, 0, -1, NULL,
    },
    {
        /* 2017, good format, check returned string */
        "170217180154Z", 1, 1, V_ASN1_UTCTIME, "170217180154Z",
    },
    {
        /* 1998, good format, check returned string */
        "981223180154Z", 1, 1, V_ASN1_UTCTIME, "981223180154Z",
    },
};

static TESTDATA x509_cmp_tests[] = {
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* The same in seconds since epoch. */
        1487354514, -1,
    },
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* One second more. */
        1487354515, -1,
    },
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* One second less. */
        1487354513, 1,
    },
    /* Same as UTC time. */
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* The same in seconds since epoch. */
        1487354514, -1,
    },
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* One second more. */
        1487354515, -1,
    },
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* One second less. */
        1487354513, 1,
    },
    /* UTCTime from the 20th century. */
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* The same in seconds since epoch. */
        919274514, -1,
    },
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* One second more. */
        919274515, -1,
    },
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* One second less. */
        919274513, 1,
    },
    /* Various invalid formats. */
    {
        /* No trailing Z. */
        "20170217180154", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* No trailing Z, UTCTime. */
        "170217180154", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* No seconds. */
        "201702171801Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* No seconds, UTCTime. */
        "1702171801Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Fractional seconds. */
        "20170217180154.001Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Fractional seconds, UTCTime. */
        "170217180154.001Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Timezone offset. */
        "20170217180154+0100", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Timezone offset, UTCTime. */
        "170217180154+0100", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Extra digits. */
        "2017021718015400Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Extra digits, UTCTime. */
        "17021718015400Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Non-digits. */
        "2017021718015aZ", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Non-digits, UTCTime. */
        "17021718015aZ", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Trailing garbage. */
        "20170217180154Zlongtrailinggarbage", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Trailing garbage, UTCTime. */
        "170217180154Zlongtrailinggarbage", V_ASN1_UTCTIME, 0, 0,
    },
    {
         /* Swapped type. */
        "20170217180154Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Swapped type. */
        "170217180154Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Bad type. */
        "20170217180154Z", V_ASN1_OCTET_STRING, 0, 0,
    },
};

static int test_x509_cmp_time(int idx)
{
    ASN1_TIME t;
    int result;

    memset(&t, 0, sizeof(t));
    t.type = x509_cmp_tests[idx].type;
    t.data = (unsigned char*)(x509_cmp_tests[idx].data);
    t.length = strlen(x509_cmp_tests[idx].data);
    t.flags = 0;

    result = X509_cmp_time(&t, &x509_cmp_tests[idx].cmp_time);
    if (!TEST_int_eq(result, x509_cmp_tests[idx].expected)) {
        TEST_info("test_x509_cmp_time(%d) failed: expected %d, got %d\n",
                idx, x509_cmp_tests[idx].expected, result);
        return 0;
    }
    return 1;
}

static int test_x509_cmp_time_current()
{
    time_t now = time(NULL);
    /* Pick a day earlier and later, relative to any system clock. */
    ASN1_TIME *asn1_before = NULL, *asn1_after = NULL;
    int cmp_result, failed = 0;

    asn1_before = ASN1_TIME_adj(NULL, now, -1, 0);
    asn1_after = ASN1_TIME_adj(NULL, now, 1, 0);

    cmp_result  = X509_cmp_time(asn1_before, NULL);
    if (!TEST_int_eq(cmp_result, -1))
        failed = 1;

    cmp_result = X509_cmp_time(asn1_after, NULL);
    if (!TEST_int_eq(cmp_result, 1))
        failed = 1;

    ASN1_TIME_free(asn1_before);
    ASN1_TIME_free(asn1_after);

    return failed == 0;
}

static int test_x509_time(int idx)
{
    ASN1_TIME *t = NULL;
    int result, rv = 0;

    if (x509_format_tests[idx].set_string) {
        /* set-string mode */
        t = ASN1_TIME_new();
        if (t == NULL) {
            TEST_info("test_x509_time(%d) failed: internal error\n", idx);
            return 0;
        }
    }

    result = ASN1_TIME_set_string_X509(t, x509_format_tests[idx].data);
    /* time string parsing result is always checked against what's expected */
    if (!TEST_int_eq(result, x509_format_tests[idx].expected)) {
        TEST_info("test_x509_time(%d) failed: expected %d, got %d\n",
                idx, x509_format_tests[idx].expected, result);
        goto out;
    }

    /* if t is not NULL but expected_type is ignored(-1), it is an 'OK' case */
    if (t != NULL && x509_format_tests[idx].expected_type != -1) {
        if (!TEST_int_eq(t->type, x509_format_tests[idx].expected_type)) {
            TEST_info("test_x509_time(%d) failed: expected_type %d, got %d\n",
                    idx, x509_format_tests[idx].expected_type, t->type);
            goto out;
        }
    }

    /* if t is not NULL but expected_string is NULL, it is an 'OK' case too */
    if (t != NULL && x509_format_tests[idx].expected_string) {
        if (!TEST_str_eq((const char *)t->data,
                    x509_format_tests[idx].expected_string)) {
            TEST_info("test_x509_time(%d) failed: expected_string %s, got %s\n",
                    idx, x509_format_tests[idx].expected_string, t->data);
            goto out;
        }
    }

    rv = 1;
out:
    if (t != NULL)
        ASN1_TIME_free(t);
    return rv;
}

void register_tests()
{
    ADD_TEST(test_x509_cmp_time_current);
    ADD_ALL_TESTS(test_x509_cmp_time, OSSL_NELEM(x509_cmp_tests));
    ADD_ALL_TESTS(test_x509_time, OSSL_NELEM(x509_format_tests));
}
