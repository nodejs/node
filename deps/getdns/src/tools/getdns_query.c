/*
 * Copyright (c) 2013, NLNet Labs, Verisign, Inc.
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
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <getdns/getdns.h>
#include <getdns/getdns_extra.h>
#ifndef USE_WINSOCK
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <iphlpapi.h>
typedef unsigned short in_port_t;
#include <windows.h>
#include <wincrypt.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_GETDNS_YAML2DICT
getdns_return_t getdns_yaml2dict(const char *, getdns_dict **dict);
#endif

#define EXAMPLE_PIN "pin-sha256=\"E9CZ9INDbd+2eRQozYqqbQ2yXLVKB9+xcprMF+44U1g=\""

static int verbosity = 0;
static int clear_listen_list_on_arg = 0;
static int quiet = 0;
static int batch_mode = 0;
static char *query_file = NULL;
static int json = 0;
static char name[2048] = ".";
static getdns_context *context;
static getdns_dict *extensions;
static getdns_dict *query_extensions_spc = NULL;
static getdns_list *pubkey_pinset = NULL;
static getdns_list *listen_list = NULL;
int touched_listen_list;
static getdns_dict *listen_dict = NULL;
static size_t pincount = 0;
static size_t listen_count = 0;
static uint16_t request_type = GETDNS_RRTYPE_NS;
static int got_rrtype = 0;
static int timeout, edns0_size, padding_blocksize;
static int async = 0, interactive = 0;
static enum { GENERAL, ADDRESS, HOSTNAME, SERVICE } calltype = GENERAL;
static int got_calltype = 0;
static int bogus_answers = 0;
static int check_dnssec = 0;
#ifndef USE_WINSOCK
static char *resolvconf = NULL;
#endif
static int print_api_info = 0, print_trust_anchors = 0;
static int log_level = 0;
static uint64_t log_systems = 0xFFFFFFFFFFFFFFFF;

static int get_rrtype(const char *t)
{
	char buf[1024] = "GETDNS_RRTYPE_";
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

static int get_rrclass(const char *t)
{
	char buf[1024] = "GETDNS_RRCLASS_";
	uint32_t rrclass;
	long int l;
	size_t i;
	char *endptr;

	if (strlen(t) > sizeof(buf) - 16)
		return -1;
	for (i = 15; *t && i < sizeof(buf) - 1; i++, t++)
		buf[i] = toupper((unsigned char)*t);
	buf[i] = '\0';

	if (!getdns_str2int(buf, &rrclass))
		return (int)rrclass;

	if (strncasecmp(buf + 15, "CLASS", 5) == 0) {
		l = strtol(buf + 20, &endptr, 10);
		if (!*endptr && l >= 0 && l < 65536)
			return l;
	}
	return -1;
}

static getdns_return_t
fill_transport_list(char *transport_list_str,
    getdns_transport_list_t *transports, size_t *transport_count)
{
	size_t max_transports = *transport_count;
	*transport_count = 0;
	for ( size_t i = 0
	    ; i < max_transports && i < strlen(transport_list_str)
	    ; i++, (*transport_count)++) {
		switch(*(transport_list_str + i)) {
			case 'U': 
				transports[i] = GETDNS_TRANSPORT_UDP;
				break;
			case 'T': 
				transports[i] = GETDNS_TRANSPORT_TCP;
				break;
			case 'L': 
				transports[i] = GETDNS_TRANSPORT_TLS;
				break;
			default:
				fprintf(stderr, "Unrecognised transport '%c' in string %s\n", 
				       *(transport_list_str + i), transport_list_str);
				return GETDNS_RETURN_GENERIC_ERROR;
		}
	}
	return GETDNS_RETURN_GOOD;
}

void
print_usage(FILE *out, const char *progname)
{
	fprintf(out, "usage: %s [<option> ...] \\\n"
	    "\t[@<upstream> ...] [+<extension> ...] [\'{ <settings> }\'] [<name>] [<type>]\n", progname);

#ifdef HAVE_LIBUNBOUND
# define DEFAULT_RESOLUTION_TYPE "recursive"
#else
# define DEFAULT_RESOLUTION_TYPE "stub"
#endif
	fprintf(out, "\ndefault mode: " DEFAULT_RESOLUTION_TYPE
            ", synchronous resolution of NS record\n\t\tusing UDP with TCP fallback\n");
	fprintf(out, "\nupstreams: @<ip>[%%<scope_id>][@<port>][#<tls port>][~<tls name>][^<tsig spec>]");
	fprintf(out, "\n            <ip>@<port> may be given as <IPv4>:<port>");
	fprintf(out, "\n                  or \'[\'<IPv6>[%%<scope_id>]\']\':<port> too\n");
	fprintf(out, "\ntsig spec: [<algorithm>:]<name>:<secret in Base64>\n");
	fprintf(out, "\nextensions:\n");
	fprintf(out, "\t+add_warning_for_bad_dns\n");
	fprintf(out, "\t+dnssec\n");
	fprintf(out, "\t+dnssec_return_status\n");
	fprintf(out, "\t+dnssec_return_only_secure\n");
	fprintf(out, "\t+dnssec_return_all_statuses\n");
	fprintf(out, "\t+dnssec_return_validation_chain\n");
	fprintf(out, "\t+dnssec_return_full_validation_chain\n");
#ifdef DNSSEC_ROADBLOCK_AVOIDANCE
	fprintf(out, "\t+dnssec_roadblock_avoidance\n");
#endif
#ifdef EDNS_COOKIES
	fprintf(out, "\t+edns_cookies\n");
#endif
	fprintf(out, "\t+return_both_v4_and_v6\n");
	fprintf(out, "\t+return_call_reporting\n");
	fprintf(out, "\t+sit=<cookie>\t\tSend along cookie OPT with value <cookie>\n");
	fprintf(out, "\t+specify_class=<class>\n");
	fprintf(out, "\t+0\t\t\tClear all extensions\n");
	fprintf(out, "\nsettings in json dict format (like outputted by -i option).\n");
	fprintf(out, "\noptions:\n");
	fprintf(out, "\t-a\tPerform asynchronous resolution (default = synchronous)\n");
	fprintf(out, "\t-A\taddress lookup (<type> is ignored)\n");
	fprintf(out, "\t-B\tBatch mode. Schedule all messages before processing responses.\n");
	fprintf(out, "\t-b <bufsize>\tSet edns0 max_udp_payload size\n");
	fprintf(out, "\t-c\tSend Client Subnet privacy request\n");
	fprintf(out, "\t-C\t<filename>\n");
	fprintf(out, "\t\tRead settings from config file <filename>\n");
	fprintf(out, "\t\tThe getdns context will be configured with these settings\n");
	fprintf(out, "\t\tThe file must be in YAML format (with extension of '.yml')\n");
	fprintf(out, "\t\tor JSON dict format (with extension '.conf')\n");
	fprintf(out, "\t-D\tSet edns0 do bit\n");
	fprintf(out, "\t-d\tclear edns0 do bit\n");
	fprintf(out, "\t-e <idle_timeout>\tSet idle timeout in milliseconds\n");
	fprintf(out, "\t-F <filename>\tread the queries from the specified file\n");
	fprintf(out, "\t-f <filename>\tRead DNSSEC trust anchors from <filename>\n");
	fprintf(out, "\t-G\tgeneral lookup\n");
	fprintf(out, "\t-H\thostname lookup. (<name> must be an IP address; <type> is ignored)\n");
	fprintf(out, "\t-h\tPrint this help\n");
	fprintf(out, "\t-i\tPrint api information\n");
	fprintf(out, "\t-I\tInteractive mode (> 1 queries on same context)\n");
	fprintf(out, "\t-j\tOutput json response dict\n");
	fprintf(out, "\t-J\tPretty print json response dict\n");
	fprintf(out, "\t-k\tPrint root trust anchors\n");
	fprintf(out, "\t-K <pin>\tPin a public key for TLS connections (can repeat)\n");
	fprintf(out, "\t\t(should look like '" EXAMPLE_PIN "')\n");
	fprintf(out, "\t-m\tSet TLS authentication mode to REQUIRED\n");
	fprintf(out, "\t-n\tSet TLS authentication mode to NONE (default)\n");
#ifndef USE_WINSOCK
	fprintf(out, "\t-o <filename>\tSet resolver configuration file path\n");
	fprintf(out, "\t\t(default = %s)\n", GETDNS_FN_RESOLVCONF);
#endif
	fprintf(out, "\t-p\tPretty print response dict (default)\n");
	fprintf(out, "\t-P <blocksize>\tPad TLS queries to a multiple of blocksize\n"
		"\t\t(special values: 0: no padding, 1: sensible default policy)\n");
	fprintf(out, "\t-q\tQuiet mode - don't print response\n");
	fprintf( out, "\t-r\tSet recursing resolution type (default = "
	    DEFAULT_RESOLUTION_TYPE ")\n");
	fprintf(out, "\t-R <filename>\tRead root hints from <filename>\n");
	fprintf(out, "\t-s\tSet stub resolution type (default = "
	    DEFAULT_RESOLUTION_TYPE ")\n");
	fprintf(out, "\t-S\tservice lookup (<type> is ignored)\n");
	fprintf(out, "\t-t <timeout>\tSet timeout in milliseconds\n");
	fprintf(out, "\t-v\tPrint getdns release version\n");
	fprintf(out, "\t-V\tIncrease verbosity (may be used more than once)\n");
	fprintf(out, "\t-x\tDo not follow redirects\n");
	fprintf(out, "\t-X\tFollow redirects (default)\n");
	fprintf(out, "\t-y <log level>\tPrint log messages with"
	    "severity <= <log level> (default = 0)\n");
	fprintf(out, "\t-Y <log systems>\tBitwise or'ed set of systems for "
	    " which to print log messages (default == -1 (= all))\n");
	fprintf(out, "\t-0\tAppend suffix to single label first (default)\n");
	fprintf(out, "\t-W\tAppend suffix always\n");
	fprintf(out, "\t-1\tAppend suffix only to single label after failure\n");
	fprintf(out, "\t-M\tAppend suffix only to multi label name after failure\n");
	fprintf(out, "\t-N\tNever append a suffix\n");
	fprintf(out, "\t-Z <suffixes>\tSet suffixes with the given comma separated list\n");

	fprintf(out, "\t-T\tSet transport to TCP only\n");
	fprintf(out, "\t-O\tSet transport to TCP only keep connections open\n");
	fprintf(out, "\t-L\tSet transport to TLS only keep connections open\n");
	fprintf(out, "\t-E\tSet transport to TLS with TCP fallback only keep connections open\n");
	fprintf(out, "\t-u\tSet transport to UDP with TCP fallback (default)\n");
	fprintf(out, "\t-U\tSet transport to UDP only\n");
	fprintf(out, "\t-l <transports>\tSet transport list. List can contain 1 of each of the characters\n");
	fprintf(out, "\t\t\t U T L for UDP, TCP or TLS e.g 'UT' or 'LTU' \n");
	fprintf(out, "\t-z <listen address>\n");
	fprintf(out, "\t\tListen for DNS requests on the given IP address\n");
	fprintf(out, "\t\t<listen address> is in the same format as upstreams.\n");
	fprintf(out, "\t\tThis option can be given more than once.\n");
}

static getdns_return_t validate_chain(getdns_dict *response)
{
	getdns_return_t r;
	getdns_list *validation_chain;
	getdns_list *replies_tree;
	getdns_dict *reply;
	getdns_list *to_validate;
	getdns_list *trust_anchor;
	size_t i;
	int s;
	
	if (!(to_validate = getdns_list_create()))
		return GETDNS_RETURN_MEMORY_ERROR;

	if (getdns_context_get_dnssec_trust_anchors(context, &trust_anchor))
		trust_anchor = getdns_root_trust_anchor(NULL);

	if ((r = getdns_dict_get_list(
	    response, "validation_chain", &validation_chain)))
		goto error;

	if ((r = getdns_dict_get_list(
	    response, "replies_tree", &replies_tree)))
		goto error;

	if (verbosity) fprintf(stdout, "replies_tree dnssec_status: ");
	switch ((s = getdns_validate_dnssec(
	    replies_tree, validation_chain, trust_anchor))) {

	case GETDNS_DNSSEC_SECURE:
		if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_SECURE\n");
		break;
	case GETDNS_DNSSEC_BOGUS:
		if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_BOGUS\n");
		bogus_answers += 1;
		break;
	case GETDNS_DNSSEC_INDETERMINATE:
		if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_INDETERMINATE\n");
		break;
	case GETDNS_DNSSEC_INSECURE:
		if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_INSECURE\n");
		break;
	case GETDNS_DNSSEC_NOT_PERFORMED:
		if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_NOT_PERFORMED\n");
		break;
	default:
		if (verbosity) fprintf(stdout, "%d\n", (int)s);
	}

	i = 0;
	while (!(r = getdns_list_get_dict(replies_tree, i++, &reply))) {

		if ((r = getdns_list_set_dict(to_validate, 0, reply)))
			goto error;

		if (verbosity) printf("reply %d, dnssec_status: ", (int)i);
		switch ((s = getdns_validate_dnssec(
		    to_validate, validation_chain, trust_anchor))) {

		case GETDNS_DNSSEC_SECURE:
			if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_SECURE\n");
			break;
		case GETDNS_DNSSEC_BOGUS:
			if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_BOGUS\n");
			bogus_answers += 1;
			break;
		case GETDNS_DNSSEC_INDETERMINATE:
			if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_INDETERMINATE\n");
			break;
		case GETDNS_DNSSEC_INSECURE:
			if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_INSECURE\n");
			break;
		case GETDNS_DNSSEC_NOT_PERFORMED:
			if (verbosity) fprintf(stdout, "GETDNS_DNSSEC_NOT_PERFORMED\n");
			break;
		default:
			if (verbosity) fprintf(stdout, "%d\n", (int)s);
		}
	}
	if (r == GETDNS_RETURN_NO_SUCH_LIST_ITEM)
		r = GETDNS_RETURN_GOOD;
error:
	getdns_list_destroy(trust_anchor);
	getdns_list_destroy(to_validate);

	return r;
}

void callback(getdns_context *context, getdns_callback_type_t callback_type,
    getdns_dict *response, void *userarg, getdns_transaction_t trans_id)
{
	char *response_str;
	(void)context; (void)userarg;

	/* This is a callback with data */;
	if (response && !quiet && (response_str = json ?
	    getdns_print_json_dict(response, json == 1)
	  : getdns_pretty_print_dict(response))) {

		fprintf(stdout, "%s\n", response_str);
		if (verbosity) fprintf(stdout, "ASYNC call completed.\n");
		validate_chain(response);
		free(response_str);
	}

	if (callback_type == GETDNS_CALLBACK_COMPLETE) {
		if (verbosity) printf("Response code was: GOOD. Status was: Callback with ID %"PRIu64"  was successful.\n",
			trans_id);
		if (check_dnssec) {
			uint32_t dnssec_status = GETDNS_DNSSEC_SECURE;

	    		(void )getdns_dict_get_int(response,
			    "/replies_tree/0/dnssec_status", &dnssec_status);
			if (dnssec_status == GETDNS_DNSSEC_BOGUS)
				bogus_answers += 1;
		}

	} else if (callback_type == GETDNS_CALLBACK_CANCEL)
		fprintf(stderr,
			"An error occurred: The callback with ID %"PRIu64" was cancelled. Exiting.\n",
			trans_id);
	else {
		fprintf(stderr,
			"An error occurred: The callback got a callback_type of %d. Exiting.\n",
			(int)callback_type);
		fprintf(stderr,
			"Error :      '%s'\n",
			getdns_get_errorstr_by_id(callback_type));
	}
	getdns_dict_destroy(response);
}

#define CONTINUE ((getdns_return_t)-2)
#define CONTINUE_ERROR ((getdns_return_t)-3)

static getdns_return_t set_cookie(getdns_dict *exts, char *cookie)
{
	uint8_t data[40];
	size_t i;
	getdns_return_t r = GETDNS_RETURN_GENERIC_ERROR;
	getdns_bindata bindata;

	getdns_dict *opt_parameters = getdns_dict_create();
	getdns_list *options = getdns_list_create();
	getdns_dict *option = getdns_dict_create();

	if (*cookie == '=')
		cookie++;

	for (i = 0; i < 40 && *cookie; i++) {
		if (*cookie >= '0' && *cookie <= '9')
			data[i] = (uint8_t)(*cookie - '0') << 4;
		else if (*cookie >= 'a' && *cookie <= 'f')
			data[i] = (uint8_t)(*cookie - 'a' + 10) << 4;
		else if (*cookie >= 'A' && *cookie <= 'F')
			data[i] = (uint8_t)(*cookie - 'A' + 10) << 4;
		else
			goto done;
		cookie++;
		if (*cookie >= '0' && *cookie <= '9')
			data[i] |= (uint8_t)(*cookie - '0');
		else if (*cookie >= 'a' && *cookie <= 'f')
			data[i] |= (uint8_t)(*cookie - 'a' + 10);
		else if (*cookie >= 'A' && *cookie <= 'F')
			data[i] |= (uint8_t)(*cookie - 'A' + 10);
		else
			goto done;
		cookie++;;
	}
	bindata.data = data;
	bindata.size = i;
	if ((r = getdns_dict_set_int(option, "option_code", 10)))
		goto done;
	if ((r = getdns_dict_set_bindata(option, "option_data", &bindata)))
		goto done;
	if ((r = getdns_list_set_dict(options, 0, option)))
		goto done;
	if ((r = getdns_dict_set_list(opt_parameters, "options", options)))
		goto done;
	r = getdns_dict_set_dict(exts, "add_opt_parameters", opt_parameters);
done:
	getdns_dict_destroy(option);
	getdns_list_destroy(options);
	getdns_dict_destroy(opt_parameters);
	return r;
}

static void parse_config(const char *config_str, int yaml_config)
{
	getdns_dict *config_dict;
	getdns_list *list;
	getdns_return_t r;

	if (yaml_config) {
#ifdef USE_YAML_CONFIG		
		r = getdns_yaml2dict(config_str, &config_dict);
#else
		fprintf(stderr, "Support for YAML configuration files not available.\n");
		return;
#endif		
	} else {
		r = getdns_str2dict(config_str, &config_dict);
	}
	if (r)
		fprintf(stderr, "Could not parse config file: %s\n",
		    getdns_get_errorstr_by_id(r));
	else {
		if (!(r = getdns_dict_get_list(
		    config_dict, "listen_addresses", &list))) {
			if (listen_list && !listen_dict) {
				getdns_list_destroy(listen_list);
				listen_list = NULL;
			}
			/* Strange construction to copy the list.
			 * Needs to be done, because config dict
			 * will get destroyed.
			 */
			if (!listen_dict &&
			    !(listen_dict = getdns_dict_create())) {
				fprintf(stderr, "Could not create "
						"listen_dict");
				r = GETDNS_RETURN_MEMORY_ERROR;

			} else if ((r = getdns_dict_set_list(
			    listen_dict, "listen_list", list)))
				fprintf(stderr, "Could not set listen_list");

			else if ((r = getdns_dict_get_list(
			    listen_dict, "listen_list", &listen_list)))
				fprintf(stderr, "Could not get listen_list");

			else if ((r = getdns_list_get_length(
			    listen_list, &listen_count)))
				fprintf(stderr, "Could not get listen_count");

			(void) getdns_dict_remove_name(
			    config_dict, "listen_addresses");

			touched_listen_list = 1;
		}
		if ((r = getdns_context_config(context, config_dict))) {
			fprintf(stderr, "Could not configure context with "
			    "config dict: %s\n", getdns_get_errorstr_by_id(r));
		}
		getdns_dict_destroy(config_dict);
	}
}

int parse_config_file(const char *fn, int report_open_failure)
{
	FILE *fh;
	char *config_file = NULL;
	long config_file_sz;
	size_t read_sz;

	if (!(fh = fopen(fn, "r"))) {
		if (report_open_failure)
			fprintf( stderr, "Could not open \"%s\": %s\n"
			       , fn, strerror(errno));
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	if (fseek(fh, 0,SEEK_END) == -1) {
		perror("fseek");
		fclose(fh);
		return GETDNS_RETURN_GENERIC_ERROR;
	}
	config_file_sz = ftell(fh);
	if (config_file_sz <= 0) {
		/* Empty config is no config */
		fclose(fh);
		return GETDNS_RETURN_GOOD;
	}
	if (!(config_file = malloc(config_file_sz + 1))){
		fclose(fh);
		fprintf(stderr, "Could not allocate memory for \"%s\"\n", fn);
		return GETDNS_RETURN_MEMORY_ERROR;
	}
	rewind(fh);
	read_sz = fread(config_file, 1, config_file_sz + 1, fh);
	if (read_sz > (size_t)config_file_sz || ferror(fh) || !feof(fh)) {
		fprintf( stderr, "An error occurred while reading \"%s\": %s\n"
		       , fn, strerror(errno));
		fclose(fh);
		return GETDNS_RETURN_MEMORY_ERROR;
	}
	config_file[read_sz] = 0;
	fclose(fh);
	parse_config(config_file, strstr(fn, ".yml") != NULL);
	free(config_file);
	return GETDNS_RETURN_GOOD;
}

getdns_return_t parse_args(int argc, char **argv)
{
	getdns_return_t r = GETDNS_RETURN_GOOD;
	size_t j;
	int i, klass;
	char *arg, *c, *endptr;
	int t;
	getdns_list *upstream_list = NULL;
	getdns_list *tas = NULL, *hints = NULL;
	getdns_dict *pubkey_pin = NULL;
	getdns_list *suffixes;
	char *suffix;
	getdns_bindata bindata;
	size_t upstream_count = 0;
	FILE *fh;
	int int_value;
	int got_qname = 0;

	for (i = 1; i < argc; i++) {
		arg = argv[i];
		if ((t = get_rrtype(arg)) >= 0) {
			request_type = t;
			got_rrtype = 1;
			continue;

		} else if (arg[0] == '+') {
			if (strncmp(arg+1, "dnssec_", 7) == 0)
				check_dnssec = 1;

			if (arg[1] == 's' && arg[2] == 'i' && arg[3] == 't' &&
			   (arg[4] == '=' || arg[4] == '\0')) {
				if ((r = set_cookie(extensions, arg+4))) {
					fprintf(stderr, "Could not set cookie:"
					    " %d", (int)r);
					break;
				}
			} else if (strncmp(arg+1, "specify_class=", 14) == 0) {
				if ((klass = get_rrclass(arg+15)) >= 0)
					r = getdns_dict_set_int(extensions,
					    "specify_class", (uint32_t )klass);
				else
					fprintf(stderr,
					    "Unknown class: %s\n", arg+15);

			} else if (arg[1] == '0') {
			    /* Unset all existing extensions*/
				getdns_dict_destroy(extensions);
				extensions = getdns_dict_create();
				break;
			} else if ((r = getdns_dict_set_int(extensions, arg+1,
			    GETDNS_EXTENSION_TRUE))) {
				fprintf(stderr, "Could not set extension "
				    "\"%s\": %d\n", argv[i], (int)r);
				break;
			}
			continue;

		} else if (arg[0] == '@') {
			getdns_dict *upstream;
			getdns_bindata *address;

			if ((r = getdns_str2dict(arg + 1, &upstream)))
				fprintf(stderr, "Could not convert \"%s\" to "
				    "an IP dict: %s\n", arg + 1,
				    getdns_get_errorstr_by_id(r));

			else if ((r = getdns_dict_get_bindata(
			    upstream, "address_data", &address))) {

				fprintf(stderr, "\"%s\" did not translate to "
				    "an IP dict: %s\n", arg + 1,
				    getdns_get_errorstr_by_id(r));

				getdns_dict_destroy(upstream);
			} else {
				if (!upstream_list &&
				    !(upstream_list =
				    getdns_list_create_with_context(context))){
					fprintf(stderr, "Could not create upstream list\n");
					return GETDNS_RETURN_MEMORY_ERROR;
				}
				(void) getdns_list_set_dict(upstream_list,
				    upstream_count++, upstream);
				getdns_dict_destroy(upstream);
			}
			continue;
		} else if (arg[0] == '{') {
			parse_config(arg, 0);
			continue;

		} else if (arg[0] != '-') {
			size_t arg_len = strlen(arg);

			got_qname = 1;
			if (arg_len > sizeof(name) - 1) {
				fprintf(stderr, "Query name too long\n");
				return GETDNS_RETURN_BAD_DOMAIN_NAME;
			}
			(void) memcpy(name, arg, arg_len);
			name[arg_len] = 0;
			continue;
		}
		for (c = arg+1; *c; c++) {
			getdns_dict *downstream;
			getdns_bindata *address;

			switch (*c) {
			case 'a':
				async = 1;
				break;
			case 'A':
				calltype = ADDRESS;
				got_calltype = 1;
				break;
			case 'b':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "max_udp_payload_size "
					    "expected after -b\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				edns0_size = strtol(argv[i], &endptr, 10);
				if (*endptr || edns0_size < 0) {
					fprintf(stderr, "positive "
					    "numeric max_udp_payload_size "
					    "expected after -b\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				getdns_context_set_edns_maximum_udp_payload_size(
				    context, (uint16_t) edns0_size);
				goto next;
			case 'c':
				if (getdns_context_set_edns_client_subnet_private(context, 1))
					return GETDNS_RETURN_GENERIC_ERROR;
				break;
			case 'C':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "file name expected "
					    "after -C\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				(void) parse_config_file(argv[i], 1);
				break;
			case 'D':
				(void) getdns_context_set_edns_do_bit(context, 1);
				break;
			case 'd':
				(void) getdns_context_set_edns_do_bit(context, 0);
				break;
			case 'f':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "file name expected "
					    "after -f\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (!(fh = fopen(argv[i], "r"))) {
					fprintf(stderr, "Could not open \"%s\""
					    ": %s\n",argv[i], strerror(errno));
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (getdns_fp2rr_list(fh, &tas, NULL, 3600)) {
					fprintf(stderr,"Could not parse "
					    "\"%s\"\n", argv[i]);
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				fclose(fh);
				if (getdns_context_set_dnssec_trust_anchors(
				    context, tas)) {
					fprintf(stderr,"Could not set "
					    "trust anchors from \"%s\"\n",
					    argv[i]);
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				getdns_list_destroy(tas);
				tas = NULL;
				break;
			case 'F':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "file name expected "
					    "after -F\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				query_file = argv[i];
				interactive = 1;
				break;
			case 'G':
				calltype = GENERAL;
				got_calltype = 1;
				break;
			case 'H':
				calltype = HOSTNAME;
				got_calltype = 1;
				break;
			case 'h':
				print_usage(stdout, argv[0]);
				return CONTINUE;
			case 'i':
				print_api_info = 1;
				break;
			case 'I':
				interactive = 1;
				break;
			case 'j':
				json = 2;
				break;
			case 'J':
				json = 1;
				break;
			case 'K':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "pin string of the form "
						EXAMPLE_PIN
						"expected after -K\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				pubkey_pin = getdns_pubkey_pin_create_from_string(context,
										 argv[i]);
				if (pubkey_pin == NULL) {
					fprintf(stderr, "could not convert '%s' into a "
						"public key pin.\n"
						"Good pins look like: " EXAMPLE_PIN "\n"
						"Please see RFC 7469 for details about "
						"the format\n", argv[i]);
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (pubkey_pinset == NULL)
					pubkey_pinset = getdns_list_create_with_context(context);
				if (r = getdns_list_set_dict(pubkey_pinset, pincount++,
							     pubkey_pin), r) {
					fprintf(stderr, "Failed to add pin to pinset (error %d: %s)\n",
						(int)r, getdns_get_errorstr_by_id(r));
					getdns_dict_destroy(pubkey_pin);
					pubkey_pin = NULL;
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				getdns_dict_destroy(pubkey_pin);
				pubkey_pin = NULL;
				break;
			case 'k':
				print_trust_anchors = 1;
				break;
			case 'n':
				getdns_context_set_tls_authentication(context,
				                 GETDNS_AUTHENTICATION_NONE);
				break;
			case 'm':
				getdns_context_set_tls_authentication(context,
				                 GETDNS_AUTHENTICATION_REQUIRED);
				break;
#ifndef USE_WINSOCK
			case 'o':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "<filename>"
					    "expected after -o\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				resolvconf = argv[i];
				break;
#endif
			case 'P':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "tls_query_padding_blocksize "
					    "expected after -P\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				padding_blocksize = strtol(argv[i], &endptr, 10);
				if (*endptr || padding_blocksize < 0) {
					fprintf(stderr, "non-negative "
					    "numeric padding blocksize expected "
					    "after -P\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (getdns_context_set_tls_query_padding_blocksize(
					    context, padding_blocksize))
					return GETDNS_RETURN_GENERIC_ERROR;
				goto next;
			case 'p':
				json = 0;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'r':
				getdns_context_set_resolution_type(
				    context,
				    GETDNS_RESOLUTION_RECURSING);
				break;
			case 'R':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "file name expected "
					    "after -f\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (!(fh = fopen(argv[i], "r"))) {
					fprintf(stderr, "Could not open \"%s\""
					    ": %s\n",argv[i], strerror(errno));
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (getdns_fp2rr_list(fh, &hints, NULL, 3600)) {
					fprintf(stderr,"Could not parse "
					    "\"%s\"\n", argv[i]);
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				fclose(fh);
				if (getdns_context_set_dns_root_servers(
				    context, hints)) {
					fprintf(stderr,"Could not set "
					    "root servers from \"%s\"\n",
					    argv[i]);
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				getdns_list_destroy(hints);
				hints = NULL;
				break;
			case 's':
				getdns_context_set_resolution_type(
				    context, GETDNS_RESOLUTION_STUB);
				break;
			case 'S':
				calltype = SERVICE;
				got_calltype = 1;
				break;
			case 't':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "timeout expected "
					    "after -t\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				timeout = strtol(argv[i], &endptr, 10);
				if (*endptr || timeout < 0) {
					fprintf(stderr, "positive "
					    "numeric timeout expected "
					    "after -t\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				getdns_context_set_timeout(
					context, timeout);
				goto next;
			case 'v':
				fprintf(stdout, "Version %s\n", GETDNS_VERSION);
				return CONTINUE;
			case 'x': 
				getdns_context_set_follow_redirects(
				    context, GETDNS_REDIRECTS_DO_NOT_FOLLOW);
				break;
			case 'X': 
				getdns_context_set_follow_redirects(
				    context, GETDNS_REDIRECTS_FOLLOW);
				break;
			case 'y':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "log level expected "
					    "after -y\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				int_value = strtol(argv[i], &endptr, 10);
				if (*endptr || int_value < 0) {
					fprintf(stderr, "positive "
					    "numeric log level expected "
					    "after -y\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				} else
					log_level = int_value;
				goto next;

			case 'Y':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "log systems expected "
					    "after -y\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				int_value = strtol(argv[i], &endptr, 10);
				if (*endptr || int_value < 0) {
					fprintf(stderr, "positive "
					    "numeric log systems expected "
					    "after -Y\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				} else
					log_systems = (uint64_t)int_value;
				goto next;

			case 'e':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "idle timeout expected "
					    "after -e\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				timeout = strtol(argv[i], &endptr, 10);
				if (*endptr || timeout < 0) {
					fprintf(stderr, "positive "
					    "numeric idle timeout expected "
					    "after -e\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				getdns_context_set_idle_timeout(
					context, timeout);
				goto next;
			case 'W':
				(void) getdns_context_set_append_name(context,
				    GETDNS_APPEND_NAME_ALWAYS);
				break;
			case '1':
				(void) getdns_context_set_append_name(context,
			GETDNS_APPEND_NAME_ONLY_TO_SINGLE_LABEL_AFTER_FAILURE);
				break;
			case '0':
				(void) getdns_context_set_append_name(context,
				    GETDNS_APPEND_NAME_TO_SINGLE_LABEL_FIRST);
				break;
			case 'M':
				(void) getdns_context_set_append_name(context,
		GETDNS_APPEND_NAME_ONLY_TO_MULTIPLE_LABEL_NAME_AFTER_FAILURE);
				break;
			case 'N':
				(void) getdns_context_set_append_name(context,
				    GETDNS_APPEND_NAME_NEVER);
				break;
			case 'Z':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "suffixes expected"
					    "after -Z\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (!(suffixes = getdns_list_create()))
					return GETDNS_RETURN_MEMORY_ERROR;
				suffix = strtok(argv[i], ",");
				j = 0;
				while (suffix) {
					bindata.size = strlen(suffix);
					bindata.data = (void *)suffix;
					(void) getdns_list_set_bindata(
					    suffixes, j++, &bindata);
					suffix = strtok(NULL, ",");
				}
				(void) getdns_context_set_suffix(context,
				    suffixes);
				getdns_list_destroy(suffixes);
				goto next;
			case 'T':
				getdns_context_set_dns_transport(context,
				    GETDNS_TRANSPORT_TCP_ONLY);
				break;
			case 'O':
				getdns_context_set_dns_transport(context,
				    GETDNS_TRANSPORT_TCP_ONLY_KEEP_CONNECTIONS_OPEN);
				break;
			case 'L':
				getdns_context_set_dns_transport(context,
				    GETDNS_TRANSPORT_TLS_ONLY_KEEP_CONNECTIONS_OPEN);
				break;
			case 'E':
				getdns_context_set_dns_transport(context,
				    GETDNS_TRANSPORT_TLS_FIRST_AND_FALL_BACK_TO_TCP_KEEP_CONNECTIONS_OPEN);
				break;
			case 'u':
				getdns_context_set_dns_transport(context,
				    GETDNS_TRANSPORT_UDP_FIRST_AND_FALL_BACK_TO_TCP);
				break;
			case 'U':
				getdns_context_set_dns_transport(context,
				    GETDNS_TRANSPORT_UDP_ONLY);
				break;
			case 'l':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "transport list expected "
					    "after -l\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				getdns_transport_list_t transports[10];
				size_t transport_count = sizeof(transports);
				if ((r = fill_transport_list(argv[i], transports, &transport_count)) ||
				    (r = getdns_context_set_dns_transport_list(context, 
				                                               transport_count, transports))){
						fprintf(stderr, "Could not set transports\n");
						return r;
				}
				break;
			case 'B':
				batch_mode = 1;
				break;
			case 'V':
				verbosity += 1;
				break;

			case 'z':
				if (c[1] != 0 || ++i >= argc || !*argv[i]) {
					fprintf(stderr, "listed address "
					                "expected after -z\n");
					return GETDNS_RETURN_GENERIC_ERROR;
				}
				if (clear_listen_list_on_arg ||
				    (argv[i][0] == '-' && argv[i][1] == '\0')) {
					if (listen_list && !listen_dict)
						getdns_list_destroy(
						    listen_list);
					listen_list = NULL;
					listen_count = 0;
					if (!clear_listen_list_on_arg) {
						touched_listen_list = 1;
						DEBUG_SERVER("Clear listen list\n");
						break;
					} else if (listen_dict) {
						getdns_dict_destroy(listen_dict);
						listen_dict = NULL;
					}
					clear_listen_list_on_arg = 0;
				}
				if ((r = getdns_str2dict(argv[i], &downstream)))
					fprintf(stderr, "Could not convert \"%s\" to "
					    "an IP dict: %s\n", argv[i],
					    getdns_get_errorstr_by_id(r));

				else if ((r = getdns_dict_get_bindata(
				    downstream, "address_data", &address))) {

					fprintf(stderr, "\"%s\" did not translate to "
					    "an IP dict: %s\n", argv[i],
					    getdns_get_errorstr_by_id(r));

					getdns_dict_destroy(downstream);
				} else {
					if (!listen_list &&
					    !(listen_list =
					    getdns_list_create_with_context(context))){
						fprintf(stderr, "Could not create "
								"downstream list\n");
						return GETDNS_RETURN_MEMORY_ERROR;
					}
					getdns_list_set_dict(listen_list,
					    listen_count++, downstream);
					getdns_dict_destroy(downstream);
					touched_listen_list = 1;
				}
				break;
			default:
				fprintf(stderr, "Unknown option "
				    "\"%c\"\n", *c);
				for (i = 0; i < argc; i++)
					fprintf(stderr, "%d: \"%s\"\n", (int)i, argv[i]);
				return GETDNS_RETURN_GENERIC_ERROR;
			}
		}
next:		;
	}
	if (!got_calltype && !got_rrtype && got_qname) {
		calltype = ADDRESS;
	}
	if (r)
		return r;
	if (pubkey_pinset && upstream_count) {
		getdns_dict *upstream;
		/* apply the accumulated pubkey pinset to all upstreams: */
		for (j = 0; j < upstream_count; j++) {
			if (r = getdns_list_get_dict(upstream_list, j, &upstream), r) {
				fprintf(stderr, "Failed to get upstream %d when adding pinset\n", (int)j);
				return r;
			}
			if (r = getdns_dict_set_list(upstream, "tls_pubkey_pinset", pubkey_pinset), r) {
				fprintf(stderr, "Failed to set pubkey pinset on upstream %d\n", (int)j);
				return r;
			}
		}
	}
	if (upstream_count &&
	    (r = getdns_context_set_upstream_recursive_servers(
	    context, upstream_list))) {
		fprintf(stderr, "Error setting upstream recursive servers\n");
	}
	if (upstream_list)
		getdns_list_destroy(upstream_list);

	return r;
}

getdns_return_t do_the_call(void)
{
	getdns_return_t r;
	getdns_dict *address = NULL;
	getdns_bindata *address_bindata;
	getdns_dict *response = NULL;
	char *response_str;
	uint32_t status;

	if (calltype != HOSTNAME)
		; /* pass */

	else if ((r = getdns_str2dict(name, &address))) {

		fprintf(stderr, "Could not convert \"%s\" to an IP dict: %s\n"
		              , name, getdns_get_errorstr_by_id(r));
		return GETDNS_RETURN_GOOD;

	} else if ((r = getdns_dict_get_bindata(
	    address, "address_data", &address_bindata))) {

		fprintf(stderr, "Could not convert \"%s\" to an IP dict: %s\n"
		              , name, getdns_get_errorstr_by_id(r));
		getdns_dict_destroy(address);
		return GETDNS_RETURN_GOOD;
	}
	if (async) {
		switch (calltype) {
		case GENERAL:
			r = getdns_general(context, name, request_type,
			    extensions, &response, NULL, callback);
			break;
		case ADDRESS:
			r = getdns_address(context, name,
			    extensions, &response, NULL, callback);
			break;
		case HOSTNAME:
			r = getdns_hostname(context, address,
			    extensions, &response, NULL, callback);
			break;
		case SERVICE:
			r = getdns_service(context, name,
			    extensions, &response, NULL, callback);
			break;
		default:
			r = GETDNS_RETURN_GENERIC_ERROR;
			break;
		}
		if (r == GETDNS_RETURN_GOOD && !batch_mode && !interactive) 
			getdns_context_run(context);
		if (r != GETDNS_RETURN_GOOD)
			fprintf(stderr, "An error occurred: %d '%s'\n", (int)r,
				 getdns_get_errorstr_by_id(r));
	} else {
		switch (calltype) {
		case GENERAL:
			r = getdns_general_sync(context, name,
			    request_type, extensions, &response);
			break;
		case ADDRESS:
			r = getdns_address_sync(context, name,
			    extensions, &response);
			break;
		case HOSTNAME:
			r = getdns_hostname_sync(context, address,
			    extensions, &response);
			break;
		case SERVICE:
			r = getdns_service_sync(context, name,
			    extensions, &response);
			break;
		default:
			r = GETDNS_RETURN_GENERIC_ERROR;
			break;
		}
		if (r != GETDNS_RETURN_GOOD) {
			fprintf(stderr, "An error occurred: %d '%s'\n", (int)r,
				 getdns_get_errorstr_by_id(r));
			getdns_dict_destroy(address);
			return r;
		}
		if (response && !quiet) {
			if ((response_str = json ?
			    getdns_print_json_dict(response, json == 1)
			  : getdns_pretty_print_dict(response))) {

				fprintf( stdout, "%s\n", response_str);
				if (verbosity) fprintf( stdout, "SYNC call completed.\n");

				validate_chain(response);
				free(response_str);
			} else {
				r = GETDNS_RETURN_MEMORY_ERROR;
				fprintf( stderr
				       , "Could not print response\n");
			}
		}
		getdns_dict_get_int(response, "status", &status);
		if (verbosity)
			fprintf(stdout, "Response code was: GOOD. Status was: %s\n", 
			 getdns_get_errorstr_by_id(status));
		if (response) {
			if (check_dnssec) {
				uint32_t dnssec_status = GETDNS_DNSSEC_SECURE;

				(void )getdns_dict_get_int(response,
				    "/replies_tree/0/dnssec_status",
				    &dnssec_status);
				if (dnssec_status == GETDNS_DNSSEC_BOGUS)
					bogus_answers += 1;
			}
			getdns_dict_destroy(response);
		}
	}
	getdns_dict_destroy(address);
	return r;
}

getdns_eventloop *loop = NULL;
FILE *fp;
static void incoming_request_handler(getdns_context *context,
    getdns_callback_type_t callback_type, getdns_dict *request,
    void *userarg, getdns_transaction_t request_id);


void read_line_cb(void *userarg);
void read_line_tiny_delay_cb(void *userarg)
{
	getdns_eventloop_event *read_line_ev = userarg;

	loop->vmt->clear(loop, read_line_ev);
	read_line_ev->timeout_cb = NULL;
	read_line_ev->read_cb = read_line_cb;
	loop->vmt->schedule(loop, fileno(fp), -1, read_line_ev);
}

void read_line_cb(void *userarg)
{
	static int n = 0;
	getdns_eventloop_event *read_line_ev = userarg;
	getdns_return_t r;

	char line[1024], *token, *linev[256];
	int linec;

	assert(n == 0);
	n += 1;
	if (!fgets(line, 1024, fp) || !*line) {
		if (query_file && verbosity)
			fprintf(stdout,"End of file.");
		loop->vmt->clear(loop, read_line_ev);
		if (listen_count)
			(void) getdns_context_set_listen_addresses(
			    context, NULL, NULL, NULL);
		if (interactive && !query_file)
			(void) getdns_context_set_upstream_recursive_servers(
			    context, NULL);
		n -= 1;
		return;
	}
	if (query_file && verbosity)
		fprintf(stdout,"Found query: %s", line);

	linev[0] = __FILE__;
	linec = 1;
	if (!(token = strtok(line, " \t\f\n\r"))) {
		if (! query_file) {
			printf("> ");
			fflush(stdout);
		}
		n -= 1;
		return;
	}
	if (*token == '#') {
		if (verbosity)
			fprintf(stdout,"Result:      Skipping comment\n");
		if (! query_file) {
			printf("> ");
			fflush(stdout);
		}
		n -= 1;
		return;
	}
	do linev[linec++] = token;
	while (linec < 256 && (token = strtok(NULL, " \t\f\n\r")));

	touched_listen_list = 0;
	r = parse_args(linec, linev);
	if (!r && touched_listen_list) {
		r = getdns_context_set_listen_addresses(
		    context, listen_list, NULL, incoming_request_handler);
	}
	if ((r || (r = do_the_call())) &&
	    (r != CONTINUE && r != CONTINUE_ERROR))
		loop->vmt->clear(loop, read_line_ev);

	else {
#if 0
		/* Tiny delay, to make sending queries less bursty with
		 * -F parameter.
		 *
		 */
		loop->vmt->clear(loop, read_line_ev);
		read_line_ev->read_cb = NULL;
		read_line_ev->timeout_cb = read_line_tiny_delay_cb;
		loop->vmt->schedule(loop, fileno(fp), 1, read_line_ev);
#endif
		if (! query_file) {
			printf("> ");
			fflush(stdout);
		}
	}
	n -= 1;
}

typedef struct dns_msg {
	getdns_transaction_t  request_id;
	getdns_dict          *request;
	getdns_resolution_t   rt;
	uint32_t              ad_bit;
	uint32_t              do_bit;
	uint32_t              cd_bit;
	int                   has_edns0;
} dns_msg;

#if defined(SERVER_DEBUG) && SERVER_DEBUG
#define SERVFAIL(error,r,msg,resp_p) do { \
	if (r)	DEBUG_SERVER("%s: %s\n", error, getdns_get_errorstr_by_id(r)); \
	else	DEBUG_SERVER("%s\n", error); \
	servfail(msg, resp_p); \
	} while (0)
#else
#define SERVFAIL(error,r,msg,resp_p) servfail(msg, resp_p)
#endif

void servfail(dns_msg *msg, getdns_dict **resp_p)
{
	getdns_dict *dict;

	if (*resp_p)
		getdns_dict_destroy(*resp_p);
	if (!(*resp_p = getdns_dict_create()))
		return;
	if (msg) {
		if (!getdns_dict_get_dict(msg->request, "header", &dict))
			getdns_dict_set_dict(*resp_p, "header", dict);
		if (!getdns_dict_get_dict(msg->request, "question", &dict))
			getdns_dict_set_dict(*resp_p, "question", dict);
		(void) getdns_dict_set_int(*resp_p, "/header/ra",
		    msg->rt == GETDNS_RESOLUTION_RECURSING ? 1 : 0);
	}
	(void) getdns_dict_set_int(
	    *resp_p, "/header/rcode", GETDNS_RCODE_SERVFAIL);
	(void) getdns_dict_set_int(*resp_p, "/header/qr", 1);
	(void) getdns_dict_set_int(*resp_p, "/header/ad", 0);
}

static getdns_return_t _handle_edns0(
    getdns_dict *response, int has_edns0)
{
	getdns_return_t r;
	getdns_list *additional;
	size_t len, i;
	getdns_dict *rr;
	uint32_t rr_type;
	char remove_str[100] = "/replies_tree/0/additional/";

	if ((r = getdns_dict_set_int(
	    response, "/replies_tree/0/header/do", 0)))
		return r;
	if ((r = getdns_dict_get_list(response, "/replies_tree/0/additional",
	    &additional)))
		return r;
	if ((r = getdns_list_get_length(additional, &len)))
		return r;
	for (i = 0; i < len; i++) {
		if ((r = getdns_list_get_dict(additional, i, &rr)))
			return r;
		if ((r = getdns_dict_get_int(rr, "type", &rr_type)))
			return r;
		if (rr_type != GETDNS_RRTYPE_OPT)
			continue;
		if (has_edns0) {
			(void) getdns_dict_set_int(rr, "do", 0);
			break;
		}
		(void) snprintf(remove_str + 27, 60, "%d", (int)i);
		if ((r = getdns_dict_remove_name(response, remove_str)))
			return r;
		break;
	}
	return GETDNS_RETURN_GOOD;
}

static void request_cb(
    getdns_context *context, getdns_callback_type_t callback_type,
    getdns_dict *response, void *userarg, getdns_transaction_t transaction_id)
{
	dns_msg *msg = (dns_msg *)userarg;
	uint32_t qid;
	getdns_return_t r = GETDNS_RETURN_GOOD;
	uint32_t n, rcode, dnssec_status = GETDNS_DNSSEC_INDETERMINATE;

#if defined(SERVER_DEBUG) && SERVER_DEBUG
	getdns_bindata *qname;
	char *qname_str, *unknown_qname = "<unknown_qname>";

	if (getdns_dict_get_bindata(msg->request, "/question/qname", &qname)
	||  getdns_convert_dns_name_to_fqdn(qname, &qname_str))
		qname_str = unknown_qname;

	DEBUG_SERVER("reply for: %p %"PRIu64" %d (edns0: %d, do: %d, ad: %d,"
	    " cd: %d, qname: %s)\n", (void *)msg, transaction_id, (int)callback_type,
	    msg->has_edns0, msg->do_bit, msg->ad_bit, msg->cd_bit, qname_str);

	if (qname_str != unknown_qname)
		free(qname_str);
#else
	(void)transaction_id;
#endif
	assert(msg);

#if 0
	fprintf(stderr, "reply: %s\n", getdns_pretty_print_dict(response));
#endif

	if (callback_type != GETDNS_CALLBACK_COMPLETE)
		SERVFAIL("Callback type not complete",
		    callback_type, msg, &response);

	else if (!response)
		SERVFAIL("Missing response", 0, msg, &response);

	else if ((r = getdns_dict_get_int(msg->request, "/header/id", &qid)) ||
	    (r=getdns_dict_set_int(response,"/replies_tree/0/header/id",qid)))
		SERVFAIL("Could not copy QID", r, msg, &response);

	else if (getdns_dict_get_int(
	    response, "/replies_tree/0/header/rcode", &rcode))
		SERVFAIL("No reply in replies tree", 0, msg, &response);

	/* ansers when CD or not BOGUS */
	else if (!msg->cd_bit && !getdns_dict_get_int(
	    response, "/replies_tree/0/dnssec_status", &dnssec_status)
	    && dnssec_status == GETDNS_DNSSEC_BOGUS)
		SERVFAIL("DNSSEC status was bogus", 0, msg, &response);

	else if (rcode == GETDNS_RCODE_SERVFAIL)
		servfail(msg, &response);

	/* RRsigs when DO and (CD or not BOGUS) 
	 * Implemented in conversion to wireformat function by checking for DO
	 * bit.  In recursing resolution mode we have to copy the do bit from
	 * the request, because libunbound has it in the answer always.
	 */
	else if (msg->rt == GETDNS_RESOLUTION_RECURSING && !msg->do_bit &&
	    (r = _handle_edns0(response, msg->has_edns0)))
		SERVFAIL("Could not handle EDNS0", r, msg, &response);

	/* AD when (DO or AD) and SECURE */
	else if ((r = getdns_dict_set_int(response,"/replies_tree/0/header/ad",
	    ((msg->do_bit || msg->ad_bit)
	    && (  (!msg->cd_bit && dnssec_status == GETDNS_DNSSEC_SECURE)
	       || ( msg->cd_bit && !getdns_dict_get_int(response,
	            "/replies_tree/0/dnssec_status", &dnssec_status)
	          && dnssec_status == GETDNS_DNSSEC_SECURE ))) ? 1 : 0)))
		SERVFAIL("Could not set AD bit", r, msg, &response);

	else if (msg->rt == GETDNS_RESOLUTION_STUB)
		; /* following checks are for RESOLUTION_RECURSING only */
	
	else if ((r =  getdns_dict_set_int(
	    response, "/replies_tree/0/header/cd", msg->cd_bit)))
		SERVFAIL("Could not copy CD bit", r, msg, &response);

	else if ((r = getdns_dict_get_int(
	    response, "/replies_tree/0/header/ra", &n)))
		SERVFAIL("Could not get RA bit from reply", r, msg, &response);

	else if (n == 0)
		SERVFAIL("Recursion not available", 0, msg, &response);

	if ((r = getdns_reply(context, response, msg->request_id))) {
		fprintf(stderr, "Could not reply: %s\n",
		    getdns_get_errorstr_by_id(r));
		/* Cancel reply */
		(void) getdns_reply(context, NULL, msg->request_id);
	}
	if (msg) {
		getdns_dict_destroy(msg->request);
		free(msg);
	}
	if (response)
		getdns_dict_destroy(response);
}	

static void incoming_request_handler(getdns_context *context,
    getdns_callback_type_t callback_type, getdns_dict *request,
    void *userarg, getdns_transaction_t request_id)
{
	getdns_bindata *qname;
	char *qname_str = NULL;
	uint32_t qtype;
	uint32_t qclass;
	getdns_return_t r;
	getdns_dict *header;
	uint32_t n;
	getdns_list *list;
	getdns_transaction_t transaction_id = 0;
	getdns_dict *qext = NULL;
	dns_msg *msg = NULL;
	getdns_dict *response = NULL;
	size_t i, len;
	getdns_list *additional;
	getdns_dict *rr;
	uint32_t rr_type;

	(void)callback_type;
	(void)userarg;

	if (!query_extensions_spc &&
	    !(query_extensions_spc = getdns_dict_create()))
		fprintf(stderr, "Could not create query extensions space\n");

	else if ((r = getdns_dict_set_dict(
	    query_extensions_spc, "qext", extensions)))
		fprintf(stderr, "Could not copy extensions in query extensions"
		                " space: %s\n", getdns_get_errorstr_by_id(r));

	else if ((r = getdns_dict_get_dict(query_extensions_spc,"qext",&qext)))
		fprintf(stderr, "Could not get query extensions from space: %s"
		              , getdns_get_errorstr_by_id(r));

	if (!(msg = malloc(sizeof(dns_msg))))
		goto error;

	/* pass through the header and the OPT record */
	n = 0;
	msg->request_id = request_id;
	msg->request = request;
	msg->ad_bit = msg->do_bit = msg->cd_bit = 0;
	msg->has_edns0 = 0;
	msg->rt = GETDNS_RESOLUTION_RECURSING;
	(void) getdns_dict_get_int(request, "/header/ad", &msg->ad_bit);
	(void) getdns_dict_get_int(request, "/header/cd", &msg->cd_bit);
	if (!getdns_dict_get_list(request, "additional", &additional)) {
		if (getdns_list_get_length(additional, &len))
			len = 0;
		for (i = 0; i < len; i++) {
			if (getdns_list_get_dict(additional, i, &rr))
				break;
			if (getdns_dict_get_int(rr, "type", &rr_type))
				break;
			if (rr_type != GETDNS_RRTYPE_OPT)
				continue;
			msg->has_edns0 = 1;
			(void) getdns_dict_get_int(rr, "do", &msg->do_bit);
			break;
		}
	}
	if ((r = getdns_context_get_resolution_type(context, &msg->rt)))
		fprintf(stderr, "Could get resolution type from context: %s\n",
		    getdns_get_errorstr_by_id(r));

	if (msg->rt == GETDNS_RESOLUTION_STUB) {
		(void)getdns_dict_set_int(
		    qext , "/add_opt_parameters/do_bit", msg->do_bit);
		if (!getdns_dict_get_dict(request, "header", &header))
			(void)getdns_dict_set_dict(qext, "header", header);

	}
	if (msg->cd_bit)
		getdns_dict_set_int(qext, "dnssec_return_all_statuses",
		    GETDNS_EXTENSION_TRUE);

	if (!getdns_dict_get_int(request, "/additional/0/extended_rcode",&n))
		(void)getdns_dict_set_int(
		    qext, "/add_opt_parameters/extended_rcode", n);

	if (!getdns_dict_get_int(request, "/additional/0/version", &n))
		(void)getdns_dict_set_int(
		    qext, "/add_opt_parameters/version", n);

	if (!getdns_dict_get_int(
	    request, "/additional/0/udp_payload_size", &n))
		(void)getdns_dict_set_int(qext,
		    "/add_opt_parameters/maximum_udp_payload_size", n);

	if (!getdns_dict_get_list(
	    request, "/additional/0/rdata/options", &list))
		(void)getdns_dict_set_list(qext,
		    "/add_opt_parameters/options", list);

#if 0
	do {
		char *str = getdns_pretty_print_dict(request);
		fprintf(stderr, "query: %s\n", str);
		free(str);
		str = getdns_pretty_print_dict(qext);
		fprintf(stderr, "query with extensions: %s\n", str);
		free(str);
	} while (0);
#endif
	if ((r = getdns_dict_get_bindata(request,"/question/qname",&qname)))
		fprintf(stderr, "Could not get qname from query: %s\n",
		    getdns_get_errorstr_by_id(r));

	else if ((r = getdns_convert_dns_name_to_fqdn(qname, &qname_str)))
		fprintf(stderr, "Could not convert qname: %s\n",
		    getdns_get_errorstr_by_id(r));

	else if ((r=getdns_dict_get_int(request,"/question/qtype",&qtype)))
		fprintf(stderr, "Could get qtype from query: %s\n",
		    getdns_get_errorstr_by_id(r));

	else if ((r=getdns_dict_get_int(request,"/question/qclass",&qclass)))
		fprintf(stderr, "Could get qclass from query: %s\n",
		    getdns_get_errorstr_by_id(r));

	else if ((r = getdns_dict_set_int(qext, "specify_class", qclass)))
		fprintf(stderr, "Could set class from query: %s\n",
		    getdns_get_errorstr_by_id(r));

	else if (qtype == GETDNS_RRTYPE_TXT && qclass == GETDNS_RRCLASS_CH &&
	    strcasecmp(qname_str, "version.bind.") == 0) {
		const char *getdns_query_version = "getdns_query " GETDNS_VERSION;
		char getdns_version[100] = "getdns ";
		char getdns_api_version[100] = "getdns API ";

		response = request;
		(void) getdns_dict_set_bindata(response, "/answer/0/name", qname);
		(void) getdns_dict_set_int(response, "/answer/0/type",  qtype);
		(void) getdns_dict_set_int(response, "/answer/0/class", qclass);
		(void) getdns_dict_set_int(response, "/answer/0/ttl",   0);
		(void) getdns_dict_util_set_string(response,
		    "/answer/0/rdata/txt_strings/0", getdns_query_version);

		(void) getdns_dict_set_bindata(response, "/answer/1/name", qname);
		(void) getdns_dict_set_int(response, "/answer/1/type",  qtype);
		(void) getdns_dict_set_int(response, "/answer/1/class", qclass);
		(void) getdns_dict_set_int(response, "/answer/1/ttl",   0);
		(void) strncat(getdns_version + 7,
		    getdns_get_version(), sizeof(getdns_version) - 8);
		(void) getdns_dict_util_set_string(response,
		    "/answer/1/rdata/txt_strings/0",getdns_version);

		(void) getdns_dict_set_bindata(response, "/answer/2/name", qname);
		(void) getdns_dict_set_int(response, "/answer/2/type",  qtype);
		(void) getdns_dict_set_int(response, "/answer/2/class", qclass);
		(void) getdns_dict_set_int(response, "/answer/2/ttl",   0);
		(void) strncat(getdns_api_version + 11,
		    getdns_get_api_version(), sizeof(getdns_api_version) - 12);
		(void) getdns_dict_util_set_string(response,
		    "/answer/2/rdata/txt_strings/0",getdns_api_version);

		(void) getdns_dict_set_int(response, "/header/ancount", 3);

		goto answer_request;

	} else if ((r = getdns_general(context, qname_str, qtype,
	    qext, msg, &transaction_id, request_cb)))
		fprintf(stderr, "Could not schedule query: %s\n",
		    getdns_get_errorstr_by_id(r));
	else {
		DEBUG_SERVER("scheduled: %p %"PRIu64" for %s %d\n",
		    (void *)msg, transaction_id, qname_str, (int)qtype);
		free(qname_str);
		return;
	}
error:
	servfail(msg, &response);
answer_request:
#if defined(SERVER_DEBUG) && SERVER_DEBUG
	do {
		char *request_str = getdns_pretty_print_dict(request);
		char *response_str = getdns_pretty_print_dict(response);
		DEBUG_SERVER("request error, request: %s\n, response: %s\n"
		            , request_str, response_str);
		free(response_str);
		free(request_str);
	} while(0);
#endif
	if ((r = getdns_reply(context, response, request_id))) {
		fprintf(stderr, "Could not reply: %s\n",
		    getdns_get_errorstr_by_id(r));
		/* Cancel reply */
		getdns_reply(context, NULL, request_id);
	}
	if (response && response != request)
		getdns_dict_destroy(response);

	if (qname_str)
		free(qname_str);

	if (msg) {
		if (msg->request)
			getdns_dict_destroy(msg->request);
		free(msg);
	}
}

static void _getdns_query_log(void *userarg, uint64_t system,
    getdns_loglevel_type level, const char *fmt, va_list ap)
{
	struct timeval tv;
	struct tm tm;
	char buf[10];
#ifdef GETDNS_ON_WINDOWS
	time_t tsec;

	gettimeofday(&tv, NULL);
	tsec = (time_t) tv.tv_sec;
	gmtime_s(&tm, (const time_t *) &tsec);
#else
	gettimeofday(&tv, NULL);
	gmtime_r(&tv.tv_sec, &tm);
#endif
	strftime(buf, 10, "%H:%M:%S", &tm);
	(void)userarg; (void)system; (void)level;
	(void) fprintf(stderr, "[%s.%.6d] UPSTREAM ", buf, (int)tv.tv_usec);
	(void) vfprintf(stderr, fmt, ap);
}


/**
 * \brief A wrapper script for command line testing of getdns
 *  getdns_query -h provides details of the available options (the syntax is 
 *  similar to that of drill)
 *  Note that for getdns the -z options enables getdns as a daemon which
 *  allows getdns to be used as the local stub (or recursive) resolver
 */

int
main(int argc, char **argv)
{
	getdns_return_t r;

	if ((r = getdns_context_create(&context, 1))) {
		fprintf(stderr, "Create context failed: %d\n", (int)r);
		return r;
	}
	if ((r = getdns_context_set_use_threads(context, 1)))
		goto done_destroy_context;
	extensions = getdns_dict_create();
	if (! extensions) {
		fprintf(stderr, "Could not create extensions dict\n");
		r = GETDNS_RETURN_MEMORY_ERROR;
		goto done_destroy_context;
	}
	if ((r = parse_args(argc, argv)) && r != CONTINUE)
		goto done_destroy_context;
#ifndef USE_WINSOCK
	if (resolvconf) {
		if ((r = getdns_context_set_resolvconf(context, resolvconf))) {
			fprintf(stderr, "Problem initializing with resolvconf: %d\n", (int)r);
			goto done_destroy_context;
		}
		if ((r = parse_args(argc, argv)))
			goto done_destroy_context;
	}
#endif
	(void) getdns_context_set_logfunc(context, NULL,
	    log_systems, log_level, _getdns_query_log);

	if (print_api_info) {
		getdns_dict *api_information = 
		    getdns_context_get_api_information(context);
		char *api_information_str;
	       
		if (listen_dict && !getdns_dict_get_list(
		    listen_dict, "listen_list", &listen_list)) {

			(void) getdns_dict_set_list(api_information,
			    "listen_addresses", listen_list);
		} else if (listen_list) {
			(void) getdns_dict_set_list(api_information,
			    "listen_addresses", listen_list);

		} else if ((listen_list = getdns_list_create())) {
			(void) getdns_dict_set_list(api_information,
			    "listen_addresses", listen_list);
			getdns_list_destroy(listen_list);
			listen_list = NULL;
		}
		api_information_str = json
		    ? getdns_print_json_dict(api_information, json == 1)
		    : getdns_pretty_print_dict(api_information);
		fprintf(stdout, "%s\n", api_information_str);
		free(api_information_str);
		getdns_dict_destroy(api_information);
	}
	if (print_trust_anchors) {
		getdns_list *tas = NULL;

		if (!getdns_context_get_dnssec_trust_anchors(context, &tas)) {
		/* if ((tas = getdns_root_trust_anchor(NULL))) { */
			char *tas_str;

			tas_str = json
			    ? getdns_print_json_list(tas, json == 1)
			    : getdns_pretty_print_list(tas);

			fprintf(stdout, "%s\n", tas_str);
			free(tas_str);
			getdns_list_destroy(tas);
		}
	}
	if (!r && (print_trust_anchors || print_api_info)) {
		r = CONTINUE;
	}
	if (r)
		goto done_destroy_context;

	clear_listen_list_on_arg = 0;

	if (query_file) {
		fp = fopen(query_file, "rt");
		if (fp == NULL) {
			fprintf(stderr, "Could not open query file: %s\n", query_file);
			goto done_destroy_context;
		}
	} else
		fp = stdin;

	if (listen_count || interactive) {
		if ((r = getdns_context_get_eventloop(context, &loop)))
			goto done_destroy_context;
		assert(loop);
	}
	if (listen_count && (r = getdns_context_set_listen_addresses(
	    context, listen_list, NULL, incoming_request_handler))) {
		perror("error: Could not bind on given addresses");
		goto done_destroy_context;
	}

	/* Make the call */
	if (interactive) {
		getdns_eventloop_event read_line_ev = {
		    &read_line_ev, read_line_cb, NULL, NULL, NULL };

		assert(loop);
		(void) loop->vmt->schedule(
		    loop, fileno(fp), -1, &read_line_ev);

		if (!query_file) {
			printf("> ");
			fflush(stdout);
		}
		loop->vmt->run(loop);
	}
	else if (listen_count) {
		assert(loop);
#ifdef SIGPIPE
		(void) signal(SIGPIPE, SIG_IGN);
#endif
		loop->vmt->run(loop);
	} else
		r = do_the_call();

	if ((r == GETDNS_RETURN_GOOD && batch_mode))
		getdns_context_run(context);

	/* Clean up */
	getdns_dict_destroy(extensions);
done_destroy_context:
	getdns_context_destroy(context);

	if (listen_list)
		getdns_list_destroy(listen_list);

	if (fp)
		fclose(fp);

	if (r == CONTINUE)
		return 0;
	else if (r == CONTINUE_ERROR)
		return 1;

	if (verbosity)
		fprintf(stdout, "\nAll done.\n");

	return             r ? r 
	     : bogus_answers ? GETDNS_DNSSEC_BOGUS
	     : GETDNS_RETURN_GOOD;
}
