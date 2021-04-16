/*
 * Copyright (c) 2018, NLNet Labs, Sinodun
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef USE_GNUTLS
#include <gnutls/x509.h>
#else
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>
#endif

#include <getdns/getdns.h>
#include <getdns/getdns_extra.h>

#define APP_NAME "getdns_server_mon"

#define RTT_CRITICAL_MS                 500
#define RTT_WARNING_MS                  250

#define CERT_EXPIRY_CRITICAL_DAYS       7
#define CERT_EXPIRY_WARNING_DAYS        14

#define DEFAULT_LOOKUP_NAME             "getdnsapi.net"
#define DEFAULT_LOOKUP_TYPE             GETDNS_RRTYPE_AAAA

#define EDNS0_PADDING_CODE              12

static const char TLS13_CIPHER_SUITE[] =
        "TLS13-AES-256-GCM-SHA384:"
        "TLS13-CHACHA20-POLY1305-SHA256:"
        "TLS13-AES-128-GCM-SHA256:"
        "TLS13-AES-128-CCM-8-SHA256:"
        "TLS13-AES-128-CCM-SHA256";

#define EXAMPLE_PIN "pin-sha256=\"E9CZ9INDbd+2eRQozYqqbQ2yXLVKB9+xcprMF+44U1g=\""

/* Plugin exit values */
typedef enum {
        EXIT_OK = 0,
        EXIT_WARNING,
        EXIT_CRITICAL,
        EXIT_UNKNOWN,
        EXIT_USAGE              /* Special case - internal only. */
} exit_value;

/* Plugin verbosity values */
typedef enum {
        VERBOSITY_MINIMAL = 0,
        VERBOSITY_ADDITIONAL,
        VERBOSITY_CONFIG,
        VERBOSITY_DEBUG
} plugin_verbosity;

#define MAX_BASE_OUTPUT_LEN             256
#define MAX_PERF_OUTPUT_LEN             256

static struct test_info_s
{
        getdns_context *context;

        /* Output */
        bool monitoring;
        FILE *errout;
        plugin_verbosity verbosity;
        bool debug_output;
        char base_output[MAX_BASE_OUTPUT_LEN + 1];
        char perf_output[MAX_PERF_OUTPUT_LEN + 1];

        /* Test config info */
        bool fail_on_dns_errors;
} test_info;

static void snprintcat(char *buf, size_t buflen, const char *format, ...)
{
        va_list ap;
        int l = strlen(buf);

        buf += l;
        buflen -= l;

        va_start(ap, format);
        vsnprintf(buf, buflen, format, ap);
        va_end(ap);
        buf[buflen] = '\0';
}

static int get_rrtype(const char *t)
{
        char buf[128] = "GETDNS_RRTYPE_";
        uint32_t rrtype;
        long int l;
        size_t i;
        char *endptr;

        if (strlen(t) > sizeof(buf) - 15)
                return -1;
        for (i = 14; *t && i < sizeof(buf) - 1; i++, t++)
                buf[i] = *t == '-' ? '_' : toupper((unsigned char)*t);
        buf[i] = '\0';

        if (!getdns_str2int(buf, &rrtype))
                return (int)rrtype;

        if (strncasecmp(buf + 14, "TYPE", 4) == 0) {
                l = strtol(buf + 18, &endptr, 10);
                if (!*endptr && l >= 0 && l < 65536)
                        return l;
        }
        return -1;
}

static const char *getdns_intval_text(int val, const char *name, const char *prefix)
{
        getdns_dict *d = getdns_dict_create();
        char buf[128];
        static char res[20];

        if (getdns_dict_set_int(d, name, val) != GETDNS_RETURN_GOOD)
                goto err;

        getdns_pretty_snprint_dict(buf, sizeof(buf), d);

        const char *p = strstr(buf, prefix);

        if (!p)
                goto err;
        p += strlen(prefix);

        char *q = res;

        while (*p && *p != '\n')
                *q++ = *p++;

        getdns_dict_destroy(d);
        return res;

err:
        getdns_dict_destroy(d);
        snprintf(res, sizeof(res), "%d", val);
        return res;
}

static const char *rrtype_text(int rrtype)
{
        return getdns_intval_text(rrtype, "type", "GETDNS_RRTYPE_");
}

static const char *rcode_text(int rcode)
{
        return getdns_intval_text(rcode, "rcode", "GETDNS_RCODE_");
}

#if !defined(USE_GNUTLS) && (OPENSSL_VERSION_NUMBER < 0x10002000 || defined(LIBRESSL_VERSION_NUMBER))
/*
 * Convert date to Julian day.
 * See https://en.wikipedia.org/wiki/Julian_day
 */
static long julian_day(const struct tm *tm)
{
        long dd, mm, yyyy;

        dd = tm->tm_mday;
        mm = tm->tm_mon + 1;
        yyyy = tm->tm_year + 1900;

        return (1461 * (yyyy + 4800 + (mm - 14) / 12)) / 4 +
                (367 * (mm - 2 - 12 * ((mm - 14) / 12))) / 12 -
                (3 * ((yyyy + 4900 + (mm - 14) / 12) / 100)) / 4 +
                dd - 32075;
}

static long secs_in_day(const struct tm *tm)
{
        return ((tm->tm_hour * 60) + tm->tm_min) * 60 + tm->tm_sec;
}
#endif

/*
 * Thanks to:
 * https://zakird.com/2013/10/13/certificate-parsing-with-openssl
 */
static bool extract_cert_expiry(const unsigned char *data, size_t len, time_t *t)
{
#ifdef USE_GNUTLS
        gnutls_x509_crt_t cert;
        gnutls_datum_t datum;
        bool res = false;

        datum.data = (unsigned char*) data;
        datum.size = len;

        if (gnutls_x509_crt_init(&cert) != GNUTLS_E_SUCCESS)
                return false;

        if (gnutls_x509_crt_import(cert, &datum, GNUTLS_X509_FMT_DER) == GNUTLS_E_SUCCESS) {
                time_t expiry = gnutls_x509_crt_get_expiration_time(cert);
                if (expiry != GNUTLS_X509_NO_WELL_DEFINED_EXPIRATION) {
                        res = true;
                        *t = expiry;
                }
        }
        gnutls_x509_crt_deinit(cert);
        return res;
#else
        X509 *cert = d2i_X509(NULL, &data, len);
        if (!cert)
                return false;

        int day_diff, sec_diff;
        const long SECS_IN_DAY = 60 * 60 * 24;
#if defined(X509_get_notAfter) || defined(HAVE_X509_GET_NOTAFTER)
        const ASN1_TIME *not_after = X509_get_notAfter(cert);
#elif defined(X509_get0_notAfter) || defined(HAVE_X509_GET0_NOTAFTER)
        const ASN1_TIME *not_after = X509_get0_notAfter(cert);
#endif
        *t = time(NULL);

#if OPENSSL_VERSION_NUMBER < 0x10002000 || defined(LIBRESSL_VERSION_NUMBER)
        /*
         * OpenSSL before 1.0.2 does not support ASN1_TIME_diff().
         * So work around by using ASN1_TIME_print() to print to a buffer
         * and parsing that. This does not do any kind of sane format,
         * but 'Mar 15 11:58:50 2018 GMT'. Note the month name is not
         * locale-dependent but always English, so strptime() to parse
         * isn't going to work. It also *appears* to always end 'GMT'.
         * Ideally one could then convert this UTC time to a time_t, but
         * there's no way to do that in standard C/POSIX. So follow the
         * lead of OpenSSL, convert to Julian days and use the difference.
         */

        char buf[40];
        BIO *b = BIO_new(BIO_s_mem());
        if (ASN1_TIME_print(b, not_after) <= 0) {
                BIO_free(b);
                X509_free(cert);
                return false;
        }
        if (BIO_gets(b, buf, sizeof(buf)) <= 0) {
                BIO_free(b);
                X509_free(cert);
                return false;
        }
        BIO_free(b);
        X509_free(cert);

        struct tm tm;
        char month[4];
        char tz[4];
        memset(&tm, 0, sizeof(tm));
        if (sscanf(buf,
                   "%3s %d %d:%d:%d %d %3s",
                   month,
                   &tm.tm_mday,
                   &tm.tm_hour,
                   &tm.tm_min,
                   &tm.tm_sec,
                   &tm.tm_year,
                   tz) != 7)
                return false;
        tm.tm_year -= 1900;
        if (strcmp(tz, "GMT") != 0)
                return false;

        const char *mon[] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };

        while(tm.tm_mon < 12 && strcmp(mon[tm.tm_mon], month) != 0)
                ++tm.tm_mon;
        if (tm.tm_mon > 11)
                return false;

        struct tm tm_now;
        gmtime_r(t, &tm_now);

        day_diff = julian_day(&tm) - julian_day(&tm_now);
        sec_diff = secs_in_day(&tm) - secs_in_day(&tm_now);
        if (sec_diff < 0) {
                sec_diff += SECS_IN_DAY;
                --day_diff;
        }
#else
        /*
         * Use ASN1_TIME_diff to get a time delta between now and expiry.
         * This is much easier than trying to parse the time.
         */
        ASN1_TIME_diff(&day_diff, &sec_diff, NULL, not_after);
        X509_free(cert);
#endif
        *t += day_diff * SECS_IN_DAY + sec_diff;
#endif /* USE_GNUTLS */
        return true;
}

static void exit_tidy()
{
        if (test_info.context)
                getdns_context_destroy(test_info.context);
}

static void usage()
{
        fputs(
"Usage: " APP_NAME " [-M] [-E] [(-u|-t|-T)] [-S] [-K <spki-pin>]\n"
"        [-v [-v [-v]]] [-V] @upstream testname [<test args>]\n"
"  -M|--monitoring               Make output suitable for monitoring tools\n"
"  -E|--fail-on-dns-errors       Fail on DNS error (NXDOMAIN, SERVFAIL)\n"
"  -u|--udp                      Use UDP transport\n"
"  -t|--tcp                      Use TCP transport\n"
"  -T|--tls                      Use TLS transport\n"
"  -S|--strict-usage-profile     Use strict profile (require authentication)\n"
"  -K|--spki-pin <spki-pin>      SPKI pin for TLS connections (can repeat)\n"
"  -v|--verbose                  Increase output verbosity\n"
"  -D|--debug                    Enable debugging output\n"
"  -V|--version                  Report GetDNS version\n"
"\n"
"spki-pin: Should look like '" EXAMPLE_PIN "'\n"
"\n"
"upstream: @<ip>[%<scope_id][@<port>][#<tls_port>][~tls name>][^<tsig spec>]\n"
"          <ip>@<port> may be given as <IPv4>:<port> or\n"
"                      '['<IPv6>[%<scope_id>]']':<port>\n"
"\n"
"tsig spec: [<algorithm>:]<name>:<secret in Base64>\n"
"\n"
"Tests:\n"
"  lookup [<name> [<type>]]      Check lookup on server\n"
"  keepalive [<name> [<type>]]   Check server support for EDNS0 keepalive in\n"
"                                TCP or TLS connections\n"
"                                Timeout of 0 is off.\n"
"  OOOR                          Check whether server delivers responses out of\n"
"                                query order on a TCP or TLS connection\n"
"  qname-min                     Check whether server supports QNAME minimisation\n"
"  rtt [warn-ms,crit-ms] [<name> [<type>]]\n"
"                                Check if server round trip time exceeds\n"
"                                thresholds (default 250,500)\n"
"\n"
"  dnssec-validate               Check whether server does DNSSEC validation\n"
"\n"
"  tls-auth [<name> [<type>]]    Check authentication of TLS server\n"
"                                If both a SPKI pin and authentication name are\n"
"                                provided, both must authenticate for this test\n"
"                                to pass.\n"
"  tls-cert-valid [warn-days,crit-days] [<name> [type]]\n"
"                                Check server certificate validity, report\n"
"                                warning or critical if days to expiry at\n"
"                                or below thresholds (default 14,7).\n"
"  tls-padding <blocksize> [<name> [<type>]]\n"
"                                Check server support for EDNS0 padding in TLS\n"
"                                Special blocksize values are 0 = off,\n"
"                                1 = sensible default.\n"
"  tls-1.3                       Check whether server supports TLS 1.3\n"
"\n"
"Enabling monitoring mode ensures output messages and exit statuses conform\n"
"to the requirements of monitoring plugins (www.monitoring-plugins.org).\n",
                test_info.errout);
        exit(EXIT_UNKNOWN);
}

static void version()
{
        fprintf(test_info.errout,
                APP_NAME ": getdns version %s, API version '%s'.\n",
                getdns_get_version(),
                getdns_get_api_version());
        exit(EXIT_UNKNOWN);
}

/**
 ** Functions used by tests.
 **/

static void get_thresholds(char ***av,
                           int *critical,
                           int *warning)
{
        if (**av) {
                char *comma = strchr(**av, ',');
                if (!comma)
                        return;

                char *end;
                long w,c;

                w = strtol(**av, &end, 10);
                /*
                 * If the number doesn't end at a comma, this isn't a
                 * properly formatted thresholds arg. Pass over it.
                 */
                if (end != comma)
                        return;

                /*
                 * Similarly, if the number doesn't end at the end of the
                 * argument, this isn't a properly formatted arg.
                 */
                c = strtol(comma + 1, &end, 10);
                if (*end != '\0')
                        return;

                /* Got two numbers, so consume the argument. */
                *critical = (int) c;
                *warning = (int) w;
                ++*av;
                return;
        }

        return;
}

static exit_value get_name_type_args(struct test_info_s *test_info,
                                     char ***av,
                                     const char **lookup_name,
                                     uint32_t *lookup_type)
{
        if (**av) {
                if (strlen(**av) > 0) {
                        *lookup_name = **av;
                } else {
                        strcpy(test_info->base_output, "Empty name not valid");
                        return EXIT_UNKNOWN;
                }
                ++*av;

                if (**av) {
                        int rrtype = get_rrtype(**av);
                        if (rrtype >= 0) {
                                *lookup_type = (uint32_t) rrtype;
                                ++*av;
                        }
                }
        }

        return EXIT_OK;
}

static exit_value search(struct test_info_s *test_info,
                         const char *name,
                         uint16_t type,
                         getdns_dict *extensions,
                         getdns_dict **response,
                         getdns_return_t *getdns_return)
{
        getdns_return_t ret;
        getdns_dict *search_extensions =
                (extensions) ? extensions : getdns_dict_create();

        /* We always turn on the return_call_reporting extension. */
        if ((ret = getdns_dict_set_int(search_extensions, "return_call_reporting", GETDNS_EXTENSION_TRUE)) != GETDNS_RETURN_GOOD) {
                if (getdns_return)
                        *getdns_return = ret;
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot set return call reporting: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                if (!extensions)
                        getdns_dict_destroy(search_extensions);
                return EXIT_UNKNOWN;
        }

        if (test_info->verbosity >= VERBOSITY_ADDITIONAL &&
            !test_info->monitoring) {
                printf("DNS Lookup name:\t%s\n", name);
                printf("DNS Lookup RR type:\t%s\n", rrtype_text(type));
        }

        if (test_info->debug_output) {
                printf("Context: %s\n",
                       getdns_pretty_print_dict(getdns_context_get_api_information(test_info->context)));
        }

        ret = getdns_general_sync(test_info->context,
                                  name,
                                  type,
                                  search_extensions,
                                  response);
        if (!extensions)
                getdns_dict_destroy(search_extensions);
        if (ret != GETDNS_RETURN_GOOD) {
                if (getdns_return) {
                        *getdns_return = ret;
                } else {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Error resolving '%s': %s (%d)",
                                 name,
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                }
                return EXIT_CRITICAL;
        }

        if (test_info->debug_output) {
                printf("Response: %s\n",
                       getdns_pretty_print_dict(*response));
        }

        if (getdns_return)
                *getdns_return = ret;
        return EXIT_OK;
}

static exit_value get_result(struct test_info_s *test_info,
                             const getdns_dict *response,
                             uint32_t *error_id,
                             uint32_t *rcode)
{
        getdns_return_t ret;

        if ((ret = getdns_dict_get_int(response, "status", error_id)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get result status: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        if (*error_id != GETDNS_RESPSTATUS_GOOD && *error_id != GETDNS_RESPSTATUS_NO_NAME) {
                *rcode = 0;
                return EXIT_OK;
        }

        if ((ret = getdns_dict_get_int(response, "/replies_tree/0/header/rcode", rcode)) !=  GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get DNS return code: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        return EXIT_OK;
}

static exit_value check_result(struct test_info_s *test_info,
                               const getdns_dict *response)
{
        exit_value xit;
        uint32_t error_id, rcode;

        if ((xit = get_result(test_info, response, &error_id, &rcode)) != EXIT_OK)
                return xit;

        switch(error_id) {
        case GETDNS_RESPSTATUS_ALL_TIMEOUT:
                strcpy(test_info->base_output, "Search timed out");
                return EXIT_CRITICAL;

        case GETDNS_RESPSTATUS_NO_SECURE_ANSWERS:
                strcpy(test_info->base_output, "No secure answers");
                return EXIT_CRITICAL;

        case GETDNS_RESPSTATUS_ALL_BOGUS_ANSWERS:
                strcpy(test_info->base_output, "All answers are bogus");
                return EXIT_CRITICAL;

        default:
                break;
        }

        if (test_info->verbosity >= VERBOSITY_ADDITIONAL){
                if (test_info->monitoring) {
                        snprintcat(test_info->perf_output,
                                   MAX_PERF_OUTPUT_LEN,
                                   "getdns=%d;",
                                   error_id);
                } else {
                        printf("getdns result:\t\t%s (%d)\n",
                                   getdns_get_errorstr_by_id(error_id),
                                   error_id);
                }
        }

        if (test_info->fail_on_dns_errors && rcode > 0) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "DNS error %s (%d)",
                         rcode_text(rcode),
                         rcode);
                return EXIT_CRITICAL;
        }

        return EXIT_OK;
}

static exit_value get_report_info(struct test_info_s *test_info,
                                  const getdns_dict *response,
                                  uint32_t *rtt,
                                  getdns_bindata **auth_status,
                                  time_t *cert_expire_time)
{
        getdns_return_t ret;
        getdns_list *l;
        uint32_t rtt_val;
        getdns_bindata *auth_status_val = NULL;
        time_t cert_expire_time_val = 0;

        if ((ret = getdns_dict_get_list(response, "call_reporting", &l)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get call report: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        getdns_dict *d;

        if ((ret = getdns_list_get_dict(l, 0, &d)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get call report first item: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }
        if (test_info->verbosity >= VERBOSITY_ADDITIONAL) {
                uint32_t transport;
                const char *transport_text = "???";

                if ((ret = getdns_dict_get_int(d, "transport", &transport)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get transport: %s (%d)",
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }

                switch(transport) {
                case GETDNS_TRANSPORT_UDP:
                        transport_text = "UDP";
                        break;

                case GETDNS_TRANSPORT_TCP:
                        transport_text = "TCP";
                        break;

                case GETDNS_TRANSPORT_TLS:
                        transport_text = "TLS";
                        break;
                }

                if (test_info->monitoring) {
                        snprintcat(test_info->perf_output,
                                   MAX_PERF_OUTPUT_LEN,
                                   "transport=%s;",
                                   transport_text);
                } else {
                        printf("Transport:\t\t%s\n", transport_text);
                }
        }

        if ((ret = getdns_dict_get_int(d, "run_time/ms", &rtt_val)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get RTT: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }
        if (rtt)
                *rtt = rtt_val;
        if (test_info->verbosity >= VERBOSITY_ADDITIONAL) {
                if (test_info->monitoring) {
                        snprintcat(test_info->perf_output,
                                   MAX_PERF_OUTPUT_LEN,
                                   "rtt=%dms;",
                                   rtt_val);
                } else {
                        printf("RTT:\t\t\t%dms\n", rtt_val);
                }
        }

        if (getdns_dict_get_bindata(d, "tls_auth_status", &auth_status_val) == GETDNS_RETURN_GOOD) {
                const char *auth_status_text = (char *) auth_status_val->data;

                /* Just in case - not sure this is necessary */
                auth_status_val->data[auth_status_val->size] = '\0';
                if (test_info->verbosity >= VERBOSITY_ADDITIONAL) {
                        if (test_info->monitoring) {
                        snprintcat(test_info->perf_output,
                                   MAX_PERF_OUTPUT_LEN,
                                   "auth=%s;",
                                   auth_status_text);
                        } else {
                                printf("Authentication:\t\t%s\n", auth_status_text);
                        }
                }
        }
        if (auth_status)
                *auth_status = auth_status_val;

        getdns_bindata *cert;

        if (getdns_dict_get_bindata(d, "tls_peer_cert", &cert) == GETDNS_RETURN_GOOD) {
                if (!extract_cert_expiry(cert->data, cert->size, &cert_expire_time_val)) {
                        strcpy(test_info->base_output, "Cannot parse PKIX certificate");
                        return EXIT_UNKNOWN;
                }
                if (test_info->verbosity >= VERBOSITY_ADDITIONAL) {
                        struct tm *tm = gmtime(&cert_expire_time_val);
                        char buf[25];
                        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
                        if (test_info->monitoring) {
                                snprintcat(test_info->perf_output,
                                           MAX_PERF_OUTPUT_LEN,
                                           "expire=%s;",
                                           buf);
                        } else {
                                printf("Certificate expires:\t%s UTC\n", buf);
                        }
                }
        }
        if (cert_expire_time)
                *cert_expire_time = cert_expire_time_val;

        getdns_bindata *tls_version;

        if (getdns_dict_get_bindata(d, "tls_version", &tls_version) == GETDNS_RETURN_GOOD) {
                const char *version_text = (char *) tls_version->data;

                if (test_info->verbosity >= VERBOSITY_ADDITIONAL) {
                        if (test_info->monitoring) {
                        snprintcat(test_info->perf_output,
                                   MAX_PERF_OUTPUT_LEN,
                                   "tls=%s;",
                                   version_text);
                        } else {
                                printf("TLS version:\t\t%s\n", version_text);
                        }
                }

                /*
                 * This is always in the context, so show only if we got
                 * TLS info in the call reporting.
                 */
                uint32_t tls_auth;
                getdns_dict *context_dict = getdns_context_get_api_information(test_info->context);

                if (getdns_dict_get_int(context_dict, "/all_context/tls_authentication", &tls_auth) == GETDNS_RETURN_GOOD) {
                        const char *auth_text = "???";

                        switch(tls_auth) {
                        case GETDNS_AUTHENTICATION_NONE:
                                auth_text = "Opportunistic";
                                break;

                        case GETDNS_AUTHENTICATION_REQUIRED:
                                auth_text = "Strict";
                                break;
                        }

                        if (test_info->verbosity >= VERBOSITY_ADDITIONAL) {
                                if (test_info->monitoring) {
                                        snprintcat(test_info->perf_output,
                                                   MAX_PERF_OUTPUT_LEN,
                                                   "%s;",
                                                   auth_text);
                                } else {
                                        printf("TLS authentication:\t%s\n", auth_text);
                                }
                        }
                }
        }

        return EXIT_OK;
}

static exit_value get_answers(struct test_info_s *test_info,
                              const getdns_dict *response,
                              const char *section,
                              getdns_list **answers,
                              size_t *no_answers)
{
        getdns_return_t ret;
        char buf[40];

        snprintf(buf, sizeof(buf), "/replies_tree/0/%s", section);

        if ((ret = getdns_dict_get_list(response, buf, answers)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get section '%s': %s (%d)",
                         section,
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        if ((ret = getdns_list_get_length(*answers, no_answers)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get number of items in '%s': %s (%d)",
                         section,
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }
        if (*no_answers <= 0) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Zero entries in '%s'",
                         section);
                return EXIT_WARNING;
        }

        return EXIT_OK;
}

static exit_value check_answer_type(struct test_info_s *test_info,
                                    const getdns_dict *response,
                                    uint32_t rrtype)
{
        getdns_list *answers;
        size_t no_answers;
        exit_value xit;

        if ((xit = get_answers(test_info, response, "answer", &answers, &no_answers)) != EXIT_OK)
                return xit;

        for (size_t i = 0; i < no_answers; ++i) {
                getdns_dict *answer;
                getdns_return_t ret;

                if ((ret = getdns_list_get_dict(answers, i, &answer)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get answer number %zu: %s (%d)",
                                 i,
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }

                uint32_t rtype;

                if ((ret = getdns_dict_get_int(answer, "type", &rtype)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get answer type: %s (%d)",
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }
                if (rtype == rrtype)
                        return EXIT_OK;
        }

        strcpy(test_info->base_output, "Answer does not contain expected type");
        return EXIT_UNKNOWN;
}

static exit_value search_check(struct test_info_s *test_info,
                               const char *lookup_name,
                               uint16_t lookup_type,
                               getdns_dict **response,
                               uint32_t *rtt,
                               getdns_bindata **auth_status,
                               time_t *cert_expire_time)
{
        exit_value xit;
        getdns_dict *resp;

        if ((xit = search(test_info, lookup_name, lookup_type, NULL, &resp, NULL)) != EXIT_OK)
                return xit;

        if ((xit = check_result(test_info, resp)) != EXIT_OK)
                return xit;

        if ((xit = get_report_info(test_info, resp, rtt, auth_status, cert_expire_time)) != EXIT_OK)
                return xit;

        if ((xit = check_answer_type(test_info, resp, lookup_type)) != EXIT_OK)
                return xit;

        if (response)
                *response = resp;
        return xit;
}

static exit_value parse_search_check(struct test_info_s *test_info,
                                     char **av,
                                     const char *usage,
                                     getdns_dict **response,
                                     uint32_t *rtt,
                                     getdns_bindata **auth_status,
                                     time_t *cert_expire_time)
{
        const char *lookup_name = DEFAULT_LOOKUP_NAME;
        uint32_t lookup_type = DEFAULT_LOOKUP_TYPE;
        exit_value xit;

        if ((xit = get_name_type_args(test_info, &av, &lookup_name, &lookup_type)) != EXIT_OK)
                return xit;

        if (*av) {
                strcpy(test_info->base_output, usage);
                return EXIT_USAGE;
        }

        return search_check(test_info,
                            lookup_name, lookup_type,
                            response,
                            rtt, auth_status, cert_expire_time);
}

/**
 ** Test routines.
 **/

static exit_value test_lookup(struct test_info_s *test_info,
                              char ** av)
{
        exit_value xit;

        if ((xit = parse_search_check(test_info,
                                      av,
                                      "lookup takes arguments [<name> [<type>]]",
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL)) != EXIT_OK)
                return xit;

        strcpy(test_info->base_output, "Lookup succeeded");
        return EXIT_OK;
}

static exit_value test_rtt(struct test_info_s *test_info,
                           char ** av)
{
        exit_value xit;
        int critical_ms = RTT_CRITICAL_MS;
        int warning_ms = RTT_WARNING_MS;
        uint32_t rtt_val;

        get_thresholds(&av, &critical_ms, &warning_ms);

        if ((xit = parse_search_check(test_info,
                                      av,
                                      "rtt takes arguments [warn-ms,crit-ms] [<name> [<type>]]",
                                      NULL,
                                      &rtt_val,
                                      NULL,
                                      NULL)) != EXIT_OK)
                return xit;

        snprintf(test_info->base_output,
                 MAX_BASE_OUTPUT_LEN,
                 "RTT lookup succeeded in %dms",
                 rtt_val);

        if ((int) rtt_val > critical_ms)
                return EXIT_CRITICAL;
        else if ((int) rtt_val > warning_ms)
                return EXIT_WARNING;
        return EXIT_OK;
}

static exit_value test_authenticate(struct test_info_s *test_info,
                                    char ** av)
{
        exit_value xit;
        getdns_bindata *auth_status;

        if ((xit = parse_search_check(test_info,
                                      av,
                                      "auth takes arguments [<name> [<type>]]",
                                      NULL,
                                      NULL,
                                      &auth_status,
                                      NULL)) != EXIT_OK)
                return xit;

        if (!auth_status || strcmp((char *) auth_status->data, "Success") != 0) {
                strcpy(test_info->base_output, "Authentication failed");
                return EXIT_CRITICAL;
        } else {
                strcpy(test_info->base_output, "Authentication succeeded");
                return EXIT_OK;
        }
}

static exit_value test_certificate_valid(struct test_info_s *test_info,
                                         char **av)
{
        exit_value xit;
        int warning_days = CERT_EXPIRY_WARNING_DAYS;
        int critical_days = CERT_EXPIRY_CRITICAL_DAYS;
        time_t expire_time;

        get_thresholds(&av, &critical_days, &warning_days);

        if ((xit = parse_search_check(test_info,
                                      av,
                                      "cert-valid takes arguments [warn-days,crit-days] [<name> [<type>]]",
                                      NULL,
                                      NULL,
                                      NULL,
                                      &expire_time)) != EXIT_OK)
                return xit;


        if (expire_time == 0) {
                strcpy(test_info->base_output, "No PKIX certificate");
                return EXIT_CRITICAL;
        }

        time_t now = time(NULL);
        int days_to_expiry = (expire_time - now) / 86400;

        if (days_to_expiry < 0) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Certificate expired %d day%s ago",
                         -days_to_expiry,
                         (days_to_expiry < -1) ? "s" : "");
                return EXIT_CRITICAL;
        }
        if (days_to_expiry == 0) {
                strcpy(test_info->base_output, "Certificate expires today");
        } else {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Certificate will expire in %d day%s",
                         days_to_expiry,
                         (days_to_expiry > 1) ? "s" : "");
        }
        if (days_to_expiry <= critical_days) {
                return EXIT_CRITICAL;
        }
        if (days_to_expiry <= warning_days) {
                return EXIT_WARNING;
        }
        return EXIT_OK;
}

static exit_value test_qname_minimisation(struct test_info_s *test_info,
                                          char ** av)
{
        if (*av) {
                strcpy(test_info->base_output, "qname-min takes no arguments");
                return EXIT_USAGE;
        }

        getdns_dict *response;
        exit_value xit;

        if ((xit = search_check(test_info,
                                "qnamemintest.internet.nl",
                                GETDNS_RRTYPE_TXT,
                                &response,
                                NULL,
                                NULL,
                                NULL)) != EXIT_OK)
                return xit;

        getdns_list *answers;
        size_t no_answers;

        if ((xit = get_answers(test_info, response, "answer", &answers, &no_answers)) != EXIT_OK)
                return xit;

        for (size_t i = 0; i < no_answers; ++i) {
                getdns_dict *answer;
                getdns_return_t ret;

                if ((ret = getdns_list_get_dict(answers, i, &answer)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get answer number %zu: %s (%d)",
                                 i,
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }

                uint32_t rtype;

                if ((ret = getdns_dict_get_int(answer, "type", &rtype)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get answer type: %s (%d)",
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }
                if (rtype != GETDNS_RRTYPE_TXT)
                        continue;

                getdns_bindata *rtxt;

                if ((ret = getdns_dict_get_bindata(answer, "/rdata/txt_strings/0", &rtxt)) != GETDNS_RETURN_GOOD) {
                        strcpy(test_info->base_output, "No answer text");
                        return EXIT_UNKNOWN;
                }

                if (rtxt->size > 0 ) {
                        switch(rtxt->data[0]) {
                        case 'H':
                                strcpy(test_info->base_output, "QNAME minimisation ON");
                                return EXIT_OK;

                        case 'N':
                                strcpy(test_info->base_output, "QNAME minimisation OFF");
                                return EXIT_CRITICAL;

                        default:
                                /* Unrecognised message. */
                                break;
                        }
                }
        }

        strcpy(test_info->base_output, "No valid QNAME minimisation data");
        return EXIT_UNKNOWN;
}

static exit_value test_padding(struct test_info_s *test_info,
                               char ** av)
{
        getdns_dict *response;
        exit_value xit;
        long blocksize;
        char *endptr;
        const char USAGE[] = "padding takes arguments <blocksize> [<name> [<type>]]";

        if (!*av || (blocksize = strtol(*av, &endptr, 10), *endptr != '\0' || blocksize < 0)) {
                strcpy(test_info->base_output, USAGE);
                return EXIT_USAGE;
        }
        ++av;

        getdns_return_t ret;
        if ((ret = getdns_context_set_tls_query_padding_blocksize(test_info->context, (uint16_t) blocksize)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot set padding blocksize: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        if ((xit = parse_search_check(test_info,
                                      av,
                                      USAGE,
                                      &response,
                                      NULL,
                                      NULL,
                                      NULL)) != EXIT_OK)
                return xit;

        getdns_list *answers;
        size_t no_answers;

        if ((xit = get_answers(test_info, response, "additional", &answers, &no_answers)) != EXIT_OK)
                return xit;

        for (size_t i = 0; i < no_answers; ++i) {
                getdns_dict *answer;
                getdns_return_t ret;

                if ((ret = getdns_list_get_dict(answers, i, &answer)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get answer number %zu: %s (%d)",
                                 i,
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }

                uint32_t rtype;

                if ((ret = getdns_dict_get_int(answer, "type", &rtype)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get answer type: %s (%d)",
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }
                if (rtype != GETDNS_RRTYPE_OPT)
                        continue;

                getdns_list *options;
                size_t no_options;

                if ((ret = getdns_dict_get_list(answer, "/rdata/options", &options)) != GETDNS_RETURN_GOOD) {
                        goto no_padding;
                }
                if ((ret = getdns_list_get_length(options, &no_options)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get number of options: %s (%d)",
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }

                for (size_t j = 0; j < no_options; ++j) {
                        getdns_dict *option;
                        uint32_t code;

                        if ((ret = getdns_list_get_dict(options, j, &option)) != GETDNS_RETURN_GOOD) {
                                snprintf(test_info->base_output,
                                         MAX_BASE_OUTPUT_LEN,
                                         "Cannot get option number %zu: %s (%d)",
                                         j,
                                         getdns_get_errorstr_by_id(ret),
                                         ret);
                                return EXIT_UNKNOWN;
                        }
                        if ((ret = getdns_dict_get_int(option, "option_code", &code)) != GETDNS_RETURN_GOOD) {
                                snprintf(test_info->base_output,
                                         MAX_BASE_OUTPUT_LEN,
                                         "Cannot get option code: %s (%d)",
                                         getdns_get_errorstr_by_id(ret),
                                         ret);
                                return EXIT_UNKNOWN;
                        }

                        if (code != EDNS0_PADDING_CODE)
                                continue;

                        /* Yes, we have padding! */
                        getdns_bindata *data;

                        if ((ret = getdns_dict_get_bindata(option, "option_data", &data)) != GETDNS_RETURN_GOOD) {
                                snprintf(test_info->base_output,
                                         MAX_BASE_OUTPUT_LEN,
                                         "Cannot get option code: %s (%d)",
                                         getdns_get_errorstr_by_id(ret),
                                         ret);
                                return EXIT_UNKNOWN;
                        }

                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Padding found, length %zu",
                                 data->size);
                        return EXIT_OK;
                }
        }

no_padding:
        strcpy(test_info->base_output, "No padding found");
        return EXIT_CRITICAL;
}

static exit_value test_keepalive(struct test_info_s *test_info,
                                 char ** av)
{
        getdns_dict *response;
        exit_value xit;
        const uint64_t MAX_TIMEOUT_VALUE = 0xffff * 10000;

        getdns_return_t ret;
        if ((ret = getdns_context_set_idle_timeout(test_info->context, MAX_TIMEOUT_VALUE)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot set keepalive timeout: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        if ((xit = parse_search_check(test_info,
                                      av,
                                      "keepalive takes arguments [<name> [<type>]]",
                                      &response,
                                      NULL,
                                      NULL,
                                      NULL)) != EXIT_OK)
                return xit;

        getdns_list *l;

        if ((ret = getdns_dict_get_list(response, "call_reporting", &l)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get call report: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        getdns_dict *d;
        if ((ret = getdns_list_get_dict(l, 0, &d)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get call report first item: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        /* Search is forced to be TCP or TLS, so server keepalive flag must exist. */
        uint32_t server_keepalive_received;
        if ((ret = getdns_dict_get_int(d, "server_keepalive_received", &server_keepalive_received)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get server keepalive flag: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        if (server_keepalive_received) {
                uint32_t t;
                bool overflow = false;

                if (!((ret = getdns_dict_get_int(d, "idle timeout in ms", &t)) == GETDNS_RETURN_GOOD ||
                      (overflow = true, ret = getdns_dict_get_int(d, "idle timeout in ms (overflow)", &t)) == GETDNS_RETURN_GOOD)) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Cannot get idle timeout: %s (%d)",
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }

                if (overflow) {
                        strcpy(test_info->base_output, "Server sent keepalive, idle timeout now (overflow)");
                } else {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Server sent keepalive, idle timeout now %ums",
                                 t);
                }
                return EXIT_OK;
        } else {
                strcpy(test_info->base_output, "Server did not send keepalive");
                return EXIT_CRITICAL;
        }
}

static exit_value test_dnssec_validate(struct test_info_s *test_info,
                                       char ** av)
{
        if (*av) {
                strcpy(test_info->base_output, "dnssec-validate takes no arguments");
                return EXIT_USAGE;
        }

        getdns_dict *response;
        exit_value xit;

        if ((xit = search(test_info,
                          "dnssec-failed.org",
                          GETDNS_RRTYPE_A,
                          NULL,
                          &response,
                          NULL)) != EXIT_OK)
                return xit;

        uint32_t error_id, rcode;

        if ((xit = get_result(test_info, response, &error_id, &rcode)) != EXIT_OK)
                return xit;

        if (error_id == GETDNS_RESPSTATUS_ALL_TIMEOUT) {
                strcpy(test_info->base_output, "Search timed out");
                return EXIT_CRITICAL;
        }

        if (rcode != GETDNS_RCODE_SERVFAIL) {
                strcpy(test_info->base_output, "Server does NOT validate DNSSEC");
                return EXIT_CRITICAL;
        }

        /*
         * Rerun the query, but this time set the CD bit. The lookup should
         * succeed.
         */
        getdns_return_t ret;
        getdns_dict *response2;
        getdns_dict *extensions = getdns_dict_create();

        if ((ret = getdns_dict_set_int(extensions, "/header/cd", 1)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot set CD bit: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                getdns_dict_destroy(extensions);
                return EXIT_UNKNOWN;
        }

        if ((xit = search(test_info,
                          "dnssec-failed.org",
                          GETDNS_RRTYPE_A,
                          extensions,
                          &response2,
                          NULL)) != EXIT_OK)
                return xit;

        getdns_dict_destroy(extensions);

        /*
         * Only now get report info from the first search, so that any
         * verbose output appears after the context/response dumps.
         */
        if ((xit = get_report_info(test_info, response, NULL, NULL, NULL)) != EXIT_OK)
                return xit;

        if ((xit = get_result(test_info, response2, &error_id, &rcode)) != EXIT_OK)
                return xit;

        if (error_id == GETDNS_RESPSTATUS_ALL_TIMEOUT) {
                strcpy(test_info->base_output, "Search timed out");
                return EXIT_CRITICAL;
        }

        if (error_id != GETDNS_RESPSTATUS_GOOD || rcode != GETDNS_RCODE_NOERROR) {
                strcpy(test_info->base_output, "Server error - cannot determine DNSSEC status");
                return EXIT_UNKNOWN;
        }

        strcpy(test_info->base_output, "Server validates DNSSEC");
        return EXIT_OK;
}

static exit_value test_tls13(struct test_info_s *test_info,
                             char ** av)
{
        exit_value xit;
        getdns_return_t ret;

        if (*av) {
                strcpy(test_info->base_output, "tls-1.3 takes no arguments");
                return EXIT_USAGE;
        }

        /*
         * Set cipher list to TLS 1.3-only ciphers. If we are using
         * an OpenSSL version that doesn't support TLS 1.3 this will cause
         * a Bad Context error on the lookup.
         */
        if ((ret = getdns_context_set_tls_cipher_list(test_info->context, TLS13_CIPHER_SUITE)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot set TLS 1.3 cipher list: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        getdns_dict *response;

        if ((xit = search(test_info,
                          DEFAULT_LOOKUP_NAME,
                          DEFAULT_LOOKUP_TYPE,
                          NULL,
                          &response,
                          &ret)) != EXIT_OK) {
                if (xit == EXIT_CRITICAL) {
                        if (ret == GETDNS_RETURN_BAD_CONTEXT) {
                                strcpy(test_info->base_output, "Your version of OpenSSL does not support TLS 1.3 ciphers. You need at least OpenSSL 1.1.1.");
                                return EXIT_UNKNOWN;
                        } else {
                                strcpy(test_info->base_output, "Cannot establish TLS 1.3 connection.");
                                return EXIT_CRITICAL;
                        }
                } else {
                        return xit;
                }
        }

        if ((xit = get_report_info(test_info, response, NULL, NULL, NULL)) != EXIT_OK)
                return xit;

        getdns_list *l;

        if ((ret = getdns_dict_get_list(response, "call_reporting", &l)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get call report: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        getdns_dict *d;

        if ((ret = getdns_list_get_dict(l, 0, &d)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get call report first item: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        /* Search is forced TLS, so tls_version flag must exist. */
        getdns_bindata *version;

        if ((ret = getdns_dict_get_bindata(d, "tls_version", &version)) != GETDNS_RETURN_GOOD) {
                snprintf(test_info->base_output,
                         MAX_BASE_OUTPUT_LEN,
                         "Cannot get TLS version: %s (%d)",
                         getdns_get_errorstr_by_id(ret),
                         ret);
                return EXIT_UNKNOWN;
        }

        const char TLS_1_3_SIG[] = "TLSv1.3";

        if (strncmp((const char *)version->data, TLS_1_3_SIG, sizeof(TLS_1_3_SIG) - 1) != 0) {
                strcpy(test_info->base_output, "Server does not support TLS 1.3");
                return EXIT_CRITICAL;
        }

        if ((xit = check_result(test_info, response)) != EXIT_OK)
                return xit;

        strcpy(test_info->base_output, "Server supports TLS 1.3");
        return EXIT_OK;
}

struct async_query
{
        const char *name;
        unsigned done_order;
        bool error;
};

static void out_of_order_callback(getdns_context          *context,
                                  getdns_callback_type_t  callback_type,
                                  getdns_dict             *response,
                                  void                    *userarg,
                                  getdns_transaction_t    transaction_id)
{
        (void) context;
        (void) callback_type;
        (void) response;
        (void) transaction_id;

        struct async_query *query = (struct async_query *) userarg;
        static unsigned callback_no;

        query->done_order = ++callback_no;
        query->error = (callback_type != GETDNS_CALLBACK_COMPLETE);

        if (test_info.verbosity >= VERBOSITY_ADDITIONAL) {
                if (test_info.monitoring) {
                        snprintcat(test_info.perf_output,
                                   MAX_PERF_OUTPUT_LEN,
                                   "->%s;",
                                   query->name);
                } else {
                        printf("Result:\t\t\t%s", query->name);
                        if (query->error)
                                printf(" (callback %d)\n", callback_type);
                        else
                                putchar('\n');
                }
        }

}

static exit_value test_out_of_order(struct test_info_s *test_info,
                                    char ** av)
{
        if (*av) {
                strcpy(test_info->base_output, "OOOR takes no arguments");
                return EXIT_USAGE;
        }

        /*
         * A set of asynchronous queries to send. One exists.
         *
         * Replies from delay.getdns.api come back with a 1s TTL. It turns out
         * that delay.getdns.api will ignore all leftmost labels bar the first,
         * so to further mitigate any cache effects insert a random string
         * into the name.
         */
        const char GOOD_NAME[] = "getdnsapi.net";
        char delay_name[50];
        srand(time(NULL));
        snprintf(delay_name, sizeof(delay_name) - 1, "400.n%04d.delay.getdnsapi.net", rand() % 10000);
        struct async_query async_queries[] = {
                { delay_name, 0, false },
                { GOOD_NAME, 0, false }
        };
        unsigned NQUERIES = sizeof(async_queries) / sizeof(async_queries[0]);

        /* Pre-load the cache with result of the good query. */
        getdns_dict *response;
        exit_value xit;

        if ((xit = search_check(test_info,
                                GOOD_NAME,
                                GETDNS_RRTYPE_AAAA,
                                &response,
                                NULL,
                                NULL,
                                NULL)) != EXIT_OK)
                return xit;

        /* Now launch async searches for all the above. */
        for (unsigned i = 0; i < NQUERIES; ++i) {
                getdns_return_t ret;

                if (test_info->verbosity >= VERBOSITY_ADDITIONAL) {
                        if (test_info->monitoring) {
                                snprintcat(test_info->perf_output,
                                           MAX_PERF_OUTPUT_LEN,
                                           "%s->;",
                                           async_queries[i].name);
                        } else {
                                printf("Search:\t\t\t%s\n", async_queries[i].name);
                        }
                }

                if ((ret = getdns_general(test_info->context,
                                          async_queries[i].name,
                                          GETDNS_RRTYPE_AAAA,
                                          NULL,
                                          &async_queries[i],
                                          NULL,
                                          out_of_order_callback)) != GETDNS_RETURN_GOOD) {
                        snprintf(test_info->base_output,
                                 MAX_BASE_OUTPUT_LEN,
                                 "Async search failed: %s (%d)",
                                 getdns_get_errorstr_by_id(ret),
                                 ret);
                        return EXIT_UNKNOWN;
                }
        }

        /* And wait for them to complete. */
        getdns_context_run(test_info->context);

        for (unsigned i = 0; i < NQUERIES; ++i) {
                if (async_queries[i].error) {
                        strcpy(test_info->base_output, "Query did not complete");
                        return EXIT_UNKNOWN;
                }
        }

        if (async_queries[NQUERIES - 1].done_order == NQUERIES) {
                strcpy(test_info->base_output, "Responses are in query order");
                return EXIT_CRITICAL;
        } else {
                strcpy(test_info->base_output, "Responses are not in query order");
                return EXIT_OK;
        }
}

static struct test_funcs_s
{
        const char *name;
        bool implies_tls;
        bool implies_tcp;
        exit_value (*func)(struct test_info_s *test_info, char **av);
} TESTS[] =
{
        { "lookup", false, false, test_lookup },
        { "rtt", false, false, test_rtt },
        { "qname-min", false, false, test_qname_minimisation },
        { "tls-auth", true, false, test_authenticate },
        { "tls-cert-valid", true, false, test_certificate_valid },
        { "tls-padding", true, false, test_padding },
        { "keepalive", false, true, test_keepalive },
        { "dnssec-validate", false, false, test_dnssec_validate },
        { "tls-1.3", true, false, test_tls13 },
        { "OOOR", false, true, test_out_of_order },
        { NULL, false, false, NULL }
};

int main(int ac, char *av[])
{
        getdns_return_t ret;
        getdns_list *pinset = NULL;
        size_t pinset_size = 0;
        bool strict_usage_profile = false;
        bool use_udp = false;
        bool use_tcp = false;
        bool use_tls = false;

        (void) ac;

        test_info.errout = stderr;
        atexit(exit_tidy);
        if ((ret = getdns_context_create(&test_info.context, 1)) != GETDNS_RETURN_GOOD) {
                fprintf(test_info.errout,
                        "Create context failed: %s (%d)\n",
                        getdns_get_errorstr_by_id(ret),
                        ret);
                exit(EXIT_UNKNOWN);
        }

        for (++av; *av && *av[0] == '-'; ++av) {
                if (strcmp(*av, "-M") == 0 ||
                    strcmp(*av, "--monitoring") == 0) {
                        test_info.monitoring = true;
                        test_info.errout = stdout;
                } else if (strcmp(*av, "-D") == 0 ||
                           strcmp(*av, "--debug") == 0) {
                        test_info.debug_output = true;
                } else if (strcmp(*av, "-E") == 0 ||
                           strcmp(*av, "--fail-on-dns-errors") == 0) {
                        test_info.fail_on_dns_errors = true;
                } else if (strcmp(*av, "-u") == 0 ||
                           strcmp(*av, "--udp") == 0 ) {
                        use_udp = true;
                } else if (strcmp(*av, "-t") == 0 ||
                           strcmp(*av, "--tcp") == 0 ) {
                        use_tcp = true;
                } else if (strcmp(*av, "-T") == 0 ||
                           strcmp(*av, "--tls") == 0 ) {
                        use_tls = true;
                } else if (strcmp(*av, "-S") == 0 ||
                           strcmp(*av, "--strict-usage-profile") == 0 ) {
                        strict_usage_profile = true;
                        use_tls = true;
                } else if (strcmp(*av, "-K") == 0 ||
                           strcmp(*av, "--spki-pin") == 0 ) {
                        ++av;
                        if (!*av) {
                                fputs("pin string of the form " EXAMPLE_PIN "expected after -K|--pin\n", test_info.errout);
                                exit(EXIT_UNKNOWN);
                        }

                        getdns_dict *pin;

                        pin = getdns_pubkey_pin_create_from_string(test_info.context, *av);
                        if (!pin) {
                                fprintf(test_info.errout,
                                        "Could not convert '%s' into a public key pin.\n"
                                        "Good pins look like: " EXAMPLE_PIN "\n"
                                        "Please see RFC 7469 for details about the format.\n", *av);
                                exit(EXIT_UNKNOWN);
                        }
                        if (!pinset)
                                pinset = getdns_list_create_with_context(test_info.context);
                        ret = getdns_list_set_dict(pinset,
                                                   pinset_size++,
                                                   pin);
                        getdns_dict_destroy(pin);
                        if (ret != GETDNS_RETURN_GOOD) {
                                fprintf(test_info.errout,
                                        "Could not add pin '%s' to pin set.\n",
                                        *av);
                                getdns_list_destroy(pinset);
                                exit(EXIT_UNKNOWN);

                        }
                        use_tls = true;
                } else if (strcmp(*av, "-v") == 0 ||
                           strcmp(*av, "--verbose") == 0) {
                        ++test_info.verbosity;
                } else if (strcmp(*av, "-V") == 0 ||
                           strcmp(*av, "--version") == 0) {
                        version();
                } else {
                        usage();
                }
        }

        if (*av == NULL || *av[0] != '@')
                usage();

        /* Non-monitoring output is chatty by default. */
        if (!test_info.monitoring)
                ++test_info.verbosity;

        const char *upstream = *av++;
        getdns_dict *resolver;
        getdns_bindata *address;

        if ((ret = getdns_str2dict(&upstream[1], &resolver)) != GETDNS_RETURN_GOOD) {
                fprintf(test_info.errout,
                        "Could not convert \"%s\" to an IP dict: %s (%d)\n",
                        &upstream[1],
                        getdns_get_errorstr_by_id(ret),
                        ret);
                exit(EXIT_UNKNOWN);
        }

        /* If the resolver info include TLS auth name, use TLS. */
        getdns_bindata *tls_auth_name;
        if (getdns_dict_get_bindata(resolver, "tls_auth_name", &tls_auth_name) == GETDNS_RETURN_GOOD)
                use_tls = true;

        if ((ret = getdns_dict_get_bindata(resolver, "address_data", &address)) != GETDNS_RETURN_GOOD) {
                fprintf(test_info.errout,
                        "\"%s\" did not translate to an IP dict: %s (%d)\n",
                        &upstream[1],
                        getdns_get_errorstr_by_id(ret),
                        ret);
                getdns_dict_destroy(resolver);
                exit(EXIT_UNKNOWN);
        }

        /* Set parameters on the resolver. */
        if (pinset) {
                ret = getdns_dict_set_list(resolver,
                                           "tls_pubkey_pinset",
                                           pinset);
                getdns_list_destroy(pinset);
                if (ret != GETDNS_RETURN_GOOD) {
                        fprintf(test_info.errout,
                                "Cannot set keys for \"%s\": %s (%d)\n",
                                &upstream[1],
                                getdns_get_errorstr_by_id(ret),
                                ret);
                        exit(EXIT_UNKNOWN);
                }
        }

        /* Set getdns context to use the indicated resolver. */
        getdns_list *l = getdns_list_create();
        ret = getdns_list_set_dict(l, 0, resolver);
        getdns_dict_destroy(resolver);
        if (ret != GETDNS_RETURN_GOOD) {
                fprintf(test_info.errout,
                        "Unable to add upstream '%s' to list: %s (%d)\n",
                        upstream,
                        getdns_get_errorstr_by_id(ret),
                        ret);
                getdns_list_destroy(l);
                exit(EXIT_UNKNOWN);
        }
        ret = getdns_context_set_upstream_recursive_servers(test_info.context, l);
        getdns_list_destroy(l);
        if (ret != GETDNS_RETURN_GOOD) {
                fprintf(test_info.errout,
                        "Unable to set upstream resolver to '%s': %s (%d)\n",
                        upstream,
                        getdns_get_errorstr_by_id(ret),
                        ret);
                exit(EXIT_UNKNOWN);
        }

        /* Set context to stub mode. */
        if ((ret = getdns_context_set_resolution_type(test_info.context, GETDNS_RESOLUTION_STUB)) != GETDNS_RETURN_GOOD) {
                fprintf(test_info.errout,
                        "Unable to set stub mode: %s (%d)\n",
                        getdns_get_errorstr_by_id(ret),
                        ret);
                exit(EXIT_UNKNOWN);
        }

        /* Set other context parameters. */
        if (strict_usage_profile) {
                ret = getdns_context_set_tls_authentication(test_info.context, GETDNS_AUTHENTICATION_REQUIRED);
                if (ret != GETDNS_RETURN_GOOD) {
                        fprintf(test_info.errout,
                                "Unable to set strict profile: %s (%d)\n",
                                getdns_get_errorstr_by_id(ret),
                                ret);
                        exit(EXIT_UNKNOWN);
                }
        }

        /* Choose and run test */
        const char *testname = *av;

        if (!testname)
                usage();
        ++av;

        const struct test_funcs_s *f;
        for (f = TESTS; f->name != NULL; ++f) {
                if (strcmp(testname, f->name) == 0)
                        break;
        }

        if (f->name == NULL) {
                fprintf(test_info.errout, "Unknown test %s\n", testname);
                exit(EXIT_UNKNOWN);
        }

        if (f->implies_tcp) {
                if (use_udp) {
                        fputs("Test requires TCP or TLS\n", test_info.errout);
                        exit(EXIT_UNKNOWN);
                }
                if (!use_tls)
                        use_tcp = true;
        }

        if (f->implies_tls) {
                if (use_udp | use_tcp) {
                        fputs("Test requires TLS, or TLS authentication specified\n", test_info.errout);
                        exit(EXIT_UNKNOWN);
                }
                use_tls = true;
        }

        if ((use_tls + use_udp + use_tcp) > 1) {
                fputs("Specify one only of -u, -t, -T\n", test_info.errout);
                exit(EXIT_UNKNOWN);
        }

        if (use_tls || use_udp || use_tcp) {
                getdns_transport_list_t udp[] = { GETDNS_TRANSPORT_UDP };
                getdns_transport_list_t tcp[] = { GETDNS_TRANSPORT_TCP };
                getdns_transport_list_t tls[] = { GETDNS_TRANSPORT_TLS };
                getdns_transport_list_t *transport =
                        (use_tls) ? tls : (use_tcp) ? tcp : udp;
                if ((ret = getdns_context_set_dns_transport_list(test_info.context, 1, transport)) != GETDNS_RETURN_GOOD) {
                        fprintf(test_info.errout,
                                "Unable to set %s transport: %s (%d)\n",
                                (use_tls) ? "TLS" : (use_tcp) ? "TCP" : "UDP",
                                getdns_get_errorstr_by_id(ret),
                                ret);
                        exit(EXIT_UNKNOWN);
                }
        }

        exit_value xit = f->func(&test_info, av);
        const char *xit_text = "(\?\?\?)";
        FILE *out = stdout;

        switch(xit) {
        case EXIT_OK:
                xit_text = "OK";
                break;

        case EXIT_WARNING:
                xit_text = "WARNING";
                break;

        case EXIT_CRITICAL:
                xit_text = "CRITICAL";
                break;

        case EXIT_UNKNOWN:
        case EXIT_USAGE:
                xit = EXIT_UNKNOWN;
                xit_text = "UNKNOWN";
                out = test_info.errout;
                break;
        }

        if (test_info.monitoring)
                fprintf(out, "DNS SERVER %s - ", xit_text);
        fputs(test_info.base_output, out);

        if (test_info.verbosity >= VERBOSITY_ADDITIONAL &&
            test_info.monitoring &&
            strlen(test_info.perf_output) > 0) {
                fprintf(out, "|%s", test_info.perf_output);
        }

        fputc('\n', out);
        exit(xit);
}
