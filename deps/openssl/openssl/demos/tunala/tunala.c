#if defined(NO_BUFFER) || defined(NO_IP) || defined(NO_OPENSSL)
#error "Badness, NO_BUFFER, NO_IP or NO_OPENSSL is defined, turn them *off*"
#endif

/* Include our bits'n'pieces */
#include "tunala.h"


/********************************************/
/* Our local types that specify our "world" */
/********************************************/

/* These represent running "tunnels". Eg. if you wanted to do SSL in a
 * "message-passing" scanario, the "int" file-descriptors might be replaced by
 * thread or process IDs, and the "select" code might be replaced by message
 * handling code. Whatever. */
typedef struct _tunala_item_t {
	/* The underlying SSL state machine. This is a data-only processing unit
	 * and we communicate with it by talking to its four "buffers". */
	state_machine_t sm;
	/* The file-descriptors for the "dirty" (encrypted) side of the SSL
	 * setup. In actuality, this is typically a socket and both values are
	 * identical. */
	int dirty_read, dirty_send;
	/* The file-descriptors for the "clean" (unencrypted) side of the SSL
	 * setup. These could be stdin/stdout, a socket (both values the same),
	 * or whatever you like. */
	int clean_read, clean_send;
} tunala_item_t;

/* This structure is used as the data for running the main loop. Namely, in a
 * network format such as this, it is stuff for select() - but as pointed out,
 * when moving the real-world to somewhere else, this might be replaced by
 * something entirely different. It's basically the stuff that controls when
 * it's time to do some "work". */
typedef struct _select_sets_t {
	int max; /* As required as the first argument to select() */
	fd_set reads, sends, excepts; /* As passed to select() */
} select_sets_t;
typedef struct _tunala_selector_t {
	select_sets_t last_selected; /* Results of the last select() */
	select_sets_t next_select; /* What we'll next select on */
} tunala_selector_t;

/* This structure is *everything*. We do it to avoid the use of globals so that,
 * for example, it would be easier to shift things around between async-IO,
 * thread-based, or multi-fork()ed (or combinations thereof). */
typedef struct _tunala_world_t {
	/* The file-descriptor we "listen" on for new connections */
	int listen_fd;
	/* The array of tunnels */
	tunala_item_t *tunnels;
	/* the number of tunnels in use and allocated, respectively */
	unsigned int tunnels_used, tunnels_size;
	/* Our outside "loop" context stuff */
	tunala_selector_t selector;
	/* Our SSL_CTX, which is configured as the SSL client or server and has
	 * the various cert-settings and callbacks configured. */
	SSL_CTX *ssl_ctx;
	/* Simple flag with complex logic :-) Indicates whether we're an SSL
	 * server or an SSL client. */
	int server_mode;
} tunala_world_t;

/*****************************/
/* Internal static functions */
/*****************************/

static SSL_CTX *initialise_ssl_ctx(int server_mode, const char *engine_id,
		const char *CAfile, const char *cert, const char *key,
		const char *dcert, const char *dkey, const char *cipher_list,
		const char *dh_file, const char *dh_special, int tmp_rsa,
		int ctx_options, int out_state, int out_verify, int verify_mode,
		unsigned int verify_depth);
static void selector_init(tunala_selector_t *selector);
static void selector_add_listener(tunala_selector_t *selector, int fd);
static void selector_add_tunala(tunala_selector_t *selector, tunala_item_t *t);
static int selector_select(tunala_selector_t *selector);
/* This returns -1 for error, 0 for no new connections, or 1 for success, in
 * which case *newfd is populated. */
static int selector_get_listener(tunala_selector_t *selector, int fd, int *newfd);
static int tunala_world_new_item(tunala_world_t *world, int fd,
		const char *ip, unsigned short port, int flipped);
static void tunala_world_del_item(tunala_world_t *world, unsigned int idx);
static int tunala_item_io(tunala_selector_t *selector, tunala_item_t *item);

/*********************************************/
/* MAIN FUNCTION (and its utility functions) */
/*********************************************/

static const char *def_proxyhost = "127.0.0.1:443";
static const char *def_listenhost = "127.0.0.1:8080";
static int def_max_tunnels = 50;
static const char *def_cacert = NULL;
static const char *def_cert = NULL;
static const char *def_key = NULL;
static const char *def_dcert = NULL;
static const char *def_dkey = NULL;
static const char *def_engine_id = NULL;
static int def_server_mode = 0;
static int def_flipped = 0;
static const char *def_cipher_list = NULL;
static const char *def_dh_file = NULL;
static const char *def_dh_special = NULL;
static int def_tmp_rsa = 1;
static int def_ctx_options = 0;
static int def_verify_mode = 0;
static unsigned int def_verify_depth = 10;
static int def_out_state = 0;
static unsigned int def_out_verify = 0;
static int def_out_totals = 0;
static int def_out_conns = 0;

static const char *helpstring =
"\n'Tunala' (A tunneler with a New Zealand accent)\n"
"Usage: tunala [options], where options are from;\n"
" -listen [host:]<port>  (default = 127.0.0.1:8080)\n"
" -proxy <host>:<port>   (default = 127.0.0.1:443)\n"
" -maxtunnels <num>      (default = 50)\n"
" -cacert <path|NULL>    (default = NULL)\n"
" -cert <path|NULL>      (default = NULL)\n"
" -key <path|NULL>       (default = whatever '-cert' is)\n"
" -dcert <path|NULL>     (usually for DSA, default = NULL)\n"
" -dkey <path|NULL>      (usually for DSA, default = whatever '-dcert' is)\n"
" -engine <id|NULL>      (default = NULL)\n"
" -server <0|1>          (default = 0, ie. an SSL client)\n"
" -flipped <0|1>         (makes SSL servers be network clients, and vice versa)\n"
" -cipher <list>         (specifies cipher list to use)\n"
" -dh_file <path>        (a PEM file containing DH parameters to use)\n"
" -dh_special <NULL|generate|standard> (see below: def=NULL)\n"
" -no_tmp_rsa            (don't generate temporary RSA keys)\n"
" -no_ssl2               (disable SSLv2)\n"
" -no_ssl3               (disable SSLv3)\n"
" -no_tls1               (disable TLSv1)\n"
" -v_peer                (verify the peer certificate)\n"
" -v_strict              (do not continue if peer doesn't authenticate)\n"
" -v_once                (no verification in renegotiates)\n"
" -v_depth <num>         (limit certificate chain depth, default = 10)\n"
" -out_conns             (prints client connections and disconnections)\n"
" -out_state             (prints SSL handshake states)\n"
" -out_verify <0|1|2|3>  (prints certificate verification states: def=1)\n"
" -out_totals            (prints out byte-totals when a tunnel closes)\n"
" -<h|help|?>            (displays this help screen)\n"
"Notes:\n"
"(1) It is recommended to specify a cert+key when operating as an SSL server.\n"
"    If you only specify '-cert', the same file must contain a matching\n"
"    private key.\n"
"(2) Either dh_file or dh_special can be used to specify where DH parameters\n"
"    will be obtained from (or '-dh_special NULL' for the default choice) but\n"
"    you cannot specify both. For dh_special, 'generate' will create new DH\n"
"    parameters on startup, and 'standard' will use embedded parameters\n"
"    instead.\n"
"(3) Normally an ssl client connects to an ssl server - so that an 'ssl client\n"
"    tunala' listens for 'clean' client connections and proxies ssl, and an\n"
"    'ssl server tunala' listens for ssl connections and proxies 'clean'. With\n"
"    '-flipped 1', this behaviour is reversed so that an 'ssl server tunala'\n"
"    listens for clean client connections and proxies ssl (but participating\n"
"    as an ssl *server* in the SSL/TLS protocol), and an 'ssl client tunala'\n"
"    listens for ssl connections (participating as an ssl *client* in the\n"
"    SSL/TLS protocol) and proxies 'clean' to the end destination. This can\n"
"    be useful for allowing network access to 'servers' where only the server\n"
"    needs to authenticate the client (ie. the other way is not required).\n"
"    Even with client and server authentication, this 'technique' mitigates\n"
"    some DoS (denial-of-service) potential as it will be the network client\n"
"    having to perform the first private key operation rather than the other\n"
"    way round.\n"
"(4) The 'technique' used by setting '-flipped 1' is probably compatible with\n"
"    absolutely nothing except another complimentary instance of 'tunala'\n"
"    running with '-flipped 1'. :-)\n";

/* Default DH parameters for use with "-dh_special standard" ... stolen striaght
 * from s_server. */
static unsigned char dh512_p[]={
	0xDA,0x58,0x3C,0x16,0xD9,0x85,0x22,0x89,0xD0,0xE4,0xAF,0x75,
	0x6F,0x4C,0xCA,0x92,0xDD,0x4B,0xE5,0x33,0xB8,0x04,0xFB,0x0F,
	0xED,0x94,0xEF,0x9C,0x8A,0x44,0x03,0xED,0x57,0x46,0x50,0xD3,
	0x69,0x99,0xDB,0x29,0xD7,0x76,0x27,0x6B,0xA2,0xD3,0xD4,0x12,
	0xE2,0x18,0xF4,0xDD,0x1E,0x08,0x4C,0xF6,0xD8,0x00,0x3E,0x7C,
	0x47,0x74,0xE8,0x33,
	};
static unsigned char dh512_g[]={
	0x02,
	};

/* And the function that parses the above "standard" parameters, again, straight
 * out of s_server. */
static DH *get_dh512(void)
	{
	DH *dh=NULL;

	if ((dh=DH_new()) == NULL) return(NULL);
	dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
	dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
		return(NULL);
	return(dh);
	}

/* Various help/error messages used by main() */
static int usage(const char *errstr, int isunknownarg)
{
	if(isunknownarg)
		fprintf(stderr, "Error: unknown argument '%s'\n", errstr);
	else
		fprintf(stderr, "Error: %s\n", errstr);
	fprintf(stderr, "%s\n", helpstring);
	return 1;
}

static int err_str0(const char *str0)
{
	fprintf(stderr, "%s\n", str0);
	return 1;
}

static int err_str1(const char *fmt, const char *str1)
{
	fprintf(stderr, fmt, str1);
	fprintf(stderr, "\n");
	return 1;
}

static int parse_max_tunnels(const char *s, unsigned int *maxtunnels)
{
	unsigned long l;
	if(!int_strtoul(s, &l) || (l < 1) || (l > 1024)) {
		fprintf(stderr, "Error, '%s' is an invalid value for "
				"maxtunnels\n", s);
		return 0;
	}
	*maxtunnels = (unsigned int)l;
	return 1;
}

static int parse_server_mode(const char *s, int *servermode)
{
	unsigned long l;
	if(!int_strtoul(s, &l) || (l > 1)) {
		fprintf(stderr, "Error, '%s' is an invalid value for the "
				"server mode\n", s);
		return 0;
	}
	*servermode = (int)l;
	return 1;
}

static int parse_dh_special(const char *s, const char **dh_special)
{
	if((strcmp(s, "NULL") == 0) || (strcmp(s, "generate") == 0) ||
			(strcmp(s, "standard") == 0)) {
		*dh_special = s;
		return 1;
	}
	fprintf(stderr, "Error, '%s' is an invalid value for 'dh_special'\n", s);
	return 0;
}

static int parse_verify_level(const char *s, unsigned int *verify_level)
{
	unsigned long l;
	if(!int_strtoul(s, &l) || (l > 3)) {
		fprintf(stderr, "Error, '%s' is an invalid value for "
				"out_verify\n", s);
		return 0;
	}
	*verify_level = (unsigned int)l;
	return 1;
}

static int parse_verify_depth(const char *s, unsigned int *verify_depth)
{
	unsigned long l;
	if(!int_strtoul(s, &l) || (l < 1) || (l > 50)) {
		fprintf(stderr, "Error, '%s' is an invalid value for "
				"verify_depth\n", s);
		return 0;
	}
	*verify_depth = (unsigned int)l;
	return 1;
}

/* Some fprintf format strings used when tunnels close */
static const char *io_stats_dirty =
"    SSL traffic;   %8lu bytes in, %8lu bytes out\n";
static const char *io_stats_clean =
"    clear traffic; %8lu bytes in, %8lu bytes out\n";

int main(int argc, char *argv[])
{
	unsigned int loop;
	int newfd;
	tunala_world_t world;
	tunala_item_t *t_item;
	const char *proxy_ip;
	unsigned short proxy_port;
	/* Overridables */
	const char *proxyhost = def_proxyhost;
	const char *listenhost = def_listenhost;
	unsigned int max_tunnels = def_max_tunnels;
	const char *cacert = def_cacert;
	const char *cert = def_cert;
	const char *key = def_key;
	const char *dcert = def_dcert;
	const char *dkey = def_dkey;
	const char *engine_id = def_engine_id;
	int server_mode = def_server_mode;
	int flipped = def_flipped;
	const char *cipher_list = def_cipher_list;
	const char *dh_file = def_dh_file;
	const char *dh_special = def_dh_special;
	int tmp_rsa = def_tmp_rsa;
	int ctx_options = def_ctx_options;
	int verify_mode = def_verify_mode;
	unsigned int verify_depth = def_verify_depth;
	int out_state = def_out_state;
	unsigned int out_verify = def_out_verify;
	int out_totals = def_out_totals;
	int out_conns = def_out_conns;

/* Parse command-line arguments */
next_arg:
	argc--; argv++;
	if(argc > 0) {
		if(strcmp(*argv, "-listen") == 0) {
			if(argc < 2)
				return usage("-listen requires an argument", 0);
			argc--; argv++;
			listenhost = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-proxy") == 0) {
			if(argc < 2)
				return usage("-proxy requires an argument", 0);
			argc--; argv++;
			proxyhost = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-maxtunnels") == 0) {
			if(argc < 2)
				return usage("-maxtunnels requires an argument", 0);
			argc--; argv++;
			if(!parse_max_tunnels(*argv, &max_tunnels))
				return 1;
			goto next_arg;
		} else if(strcmp(*argv, "-cacert") == 0) {
			if(argc < 2)
				return usage("-cacert requires an argument", 0);
			argc--; argv++;
			if(strcmp(*argv, "NULL") == 0)
				cacert = NULL;
			else
				cacert = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-cert") == 0) {
			if(argc < 2)
				return usage("-cert requires an argument", 0);
			argc--; argv++;
			if(strcmp(*argv, "NULL") == 0)
				cert = NULL;
			else
				cert = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-key") == 0) {
			if(argc < 2)
				return usage("-key requires an argument", 0);
			argc--; argv++;
			if(strcmp(*argv, "NULL") == 0)
				key = NULL;
			else
				key = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-dcert") == 0) {
			if(argc < 2)
				return usage("-dcert requires an argument", 0);
			argc--; argv++;
			if(strcmp(*argv, "NULL") == 0)
				dcert = NULL;
			else
				dcert = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-dkey") == 0) {
			if(argc < 2)
				return usage("-dkey requires an argument", 0);
			argc--; argv++;
			if(strcmp(*argv, "NULL") == 0)
				dkey = NULL;
			else
				dkey = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-engine") == 0) {
			if(argc < 2)
				return usage("-engine requires an argument", 0);
			argc--; argv++;
			engine_id = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-server") == 0) {
			if(argc < 2)
				return usage("-server requires an argument", 0);
			argc--; argv++;
			if(!parse_server_mode(*argv, &server_mode))
				return 1;
			goto next_arg;
		} else if(strcmp(*argv, "-flipped") == 0) {
			if(argc < 2)
				return usage("-flipped requires an argument", 0);
			argc--; argv++;
			if(!parse_server_mode(*argv, &flipped))
				return 1;
			goto next_arg;
		} else if(strcmp(*argv, "-cipher") == 0) {
			if(argc < 2)
				return usage("-cipher requires an argument", 0);
			argc--; argv++;
			cipher_list = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-dh_file") == 0) {
			if(argc < 2)
				return usage("-dh_file requires an argument", 0);
			if(dh_special)
				return usage("cannot mix -dh_file with "
						"-dh_special", 0);
			argc--; argv++;
			dh_file = *argv;
			goto next_arg;
		} else if(strcmp(*argv, "-dh_special") == 0) {
			if(argc < 2)
				return usage("-dh_special requires an argument", 0);
			if(dh_file)
				return usage("cannot mix -dh_file with "
						"-dh_special", 0);
			argc--; argv++;
			if(!parse_dh_special(*argv, &dh_special))
				return 1;
			goto next_arg;
		} else if(strcmp(*argv, "-no_tmp_rsa") == 0) {
			tmp_rsa = 0;
			goto next_arg;
		} else if(strcmp(*argv, "-no_ssl2") == 0) {
			ctx_options |= SSL_OP_NO_SSLv2;
			goto next_arg;
		} else if(strcmp(*argv, "-no_ssl3") == 0) {
			ctx_options |= SSL_OP_NO_SSLv3;
			goto next_arg;
		} else if(strcmp(*argv, "-no_tls1") == 0) {
			ctx_options |= SSL_OP_NO_TLSv1;
			goto next_arg;
		} else if(strcmp(*argv, "-v_peer") == 0) {
			verify_mode |= SSL_VERIFY_PEER;
			goto next_arg;
		} else if(strcmp(*argv, "-v_strict") == 0) {
			verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
			goto next_arg;
		} else if(strcmp(*argv, "-v_once") == 0) {
			verify_mode |= SSL_VERIFY_CLIENT_ONCE;
			goto next_arg;
		} else if(strcmp(*argv, "-v_depth") == 0) {
			if(argc < 2)
				return usage("-v_depth requires an argument", 0);
			argc--; argv++;
			if(!parse_verify_depth(*argv, &verify_depth))
				return 1;
			goto next_arg;
		} else if(strcmp(*argv, "-out_state") == 0) {
			out_state = 1;
			goto next_arg;
		} else if(strcmp(*argv, "-out_verify") == 0) {
			if(argc < 2)
				return usage("-out_verify requires an argument", 0);
			argc--; argv++;
			if(!parse_verify_level(*argv, &out_verify))
				return 1;
			goto next_arg;
		} else if(strcmp(*argv, "-out_totals") == 0) {
			out_totals = 1;
			goto next_arg;
		} else if(strcmp(*argv, "-out_conns") == 0) {
			out_conns = 1;
			goto next_arg;
		} else if((strcmp(*argv, "-h") == 0) ||
				(strcmp(*argv, "-help") == 0) ||
				(strcmp(*argv, "-?") == 0)) {
			fprintf(stderr, "%s\n", helpstring);
			return 0;
		} else
			return usage(*argv, 1);
	}
	/* Run any sanity checks we want here */
	if(!cert && !dcert && server_mode)
		fprintf(stderr, "WARNING: you are running an SSL server without "
				"a certificate - this may not work!\n");

	/* Initialise network stuff */
	if(!ip_initialise())
		return err_str0("ip_initialise failed");
	/* Create the SSL_CTX */
	if((world.ssl_ctx = initialise_ssl_ctx(server_mode, engine_id,
			cacert, cert, key, dcert, dkey, cipher_list, dh_file,
			dh_special, tmp_rsa, ctx_options, out_state, out_verify,
			verify_mode, verify_depth)) == NULL)
		return err_str1("initialise_ssl_ctx(engine_id=%s) failed",
			(engine_id == NULL) ? "NULL" : engine_id);
	if(engine_id)
		fprintf(stderr, "Info, engine '%s' initialised\n", engine_id);
	/* Create the listener */
	if((world.listen_fd = ip_create_listener(listenhost)) == -1)
		return err_str1("ip_create_listener(%s) failed", listenhost);
	fprintf(stderr, "Info, listening on '%s'\n", listenhost);
	if(!ip_parse_address(proxyhost, &proxy_ip, &proxy_port, 0))
		return err_str1("ip_parse_address(%s) failed", proxyhost);
	fprintf(stderr, "Info, proxying to '%s' (%d.%d.%d.%d:%d)\n", proxyhost,
			(int)proxy_ip[0], (int)proxy_ip[1],
			(int)proxy_ip[2], (int)proxy_ip[3], (int)proxy_port);
	fprintf(stderr, "Info, set maxtunnels to %d\n", (int)max_tunnels);
	fprintf(stderr, "Info, set to operate as an SSL %s\n",
			(server_mode ? "server" : "client"));
	/* Initialise the rest of the stuff */
	world.tunnels_used = world.tunnels_size = 0;
	world.tunnels = NULL;
	world.server_mode = server_mode;
	selector_init(&world.selector);

/* We're ready to loop */
main_loop:
	/* Should we listen for *new* tunnels? */
	if(world.tunnels_used < max_tunnels)
		selector_add_listener(&world.selector, world.listen_fd);
	/* We should add in our existing tunnels */
	for(loop = 0; loop < world.tunnels_used; loop++)
		selector_add_tunala(&world.selector, world.tunnels + loop);
	/* Now do the select */
	switch(selector_select(&world.selector)) {
	case -1:
		if(errno != EINTR) {
			fprintf(stderr, "selector_select returned a "
					"badness error.\n");
			goto shouldnt_happen;
		}
		fprintf(stderr, "Warn, selector interrupted by a signal\n");
		goto main_loop;
	case 0:
		fprintf(stderr, "Warn, selector_select returned 0 - signal?""?\n");
		goto main_loop;
	default:
		break;
	}
	/* Accept new connection if we should and can */
	if((world.tunnels_used < max_tunnels) && (selector_get_listener(
					&world.selector, world.listen_fd,
					&newfd) == 1)) {
		/* We have a new connection */
		if(!tunala_world_new_item(&world, newfd, proxy_ip,
						proxy_port, flipped))
			fprintf(stderr, "tunala_world_new_item failed\n");
		else if(out_conns)
			fprintf(stderr, "Info, new tunnel opened, now up to "
					"%d\n", world.tunnels_used);
	}
	/* Give each tunnel its moment, note the while loop is because it makes
	 * the logic easier than with "for" to deal with an array that may shift
	 * because of deletes. */
	loop = 0;
	t_item = world.tunnels;
	while(loop < world.tunnels_used) {
		if(!tunala_item_io(&world.selector, t_item)) {
			/* We're closing whether for reasons of an error or a
			 * natural close. Don't increment loop or t_item because
			 * the next item is moving to us! */
			if(!out_totals)
				goto skip_totals;
			fprintf(stderr, "Tunnel closing, traffic stats follow\n");
			/* Display the encrypted (over the network) stats */
			fprintf(stderr, io_stats_dirty,
				buffer_total_in(state_machine_get_buffer(
						&t_item->sm,SM_DIRTY_IN)),
				buffer_total_out(state_machine_get_buffer(
						&t_item->sm,SM_DIRTY_OUT)));
			/* Display the local (tunnelled) stats. NB: Data we
			 * *receive* is data sent *out* of the state_machine on
			 * its 'clean' side. Hence the apparent back-to-front
			 * OUT/IN mixup here :-) */
			fprintf(stderr, io_stats_clean,
				buffer_total_out(state_machine_get_buffer(
						&t_item->sm,SM_CLEAN_OUT)),
				buffer_total_in(state_machine_get_buffer(
						&t_item->sm,SM_CLEAN_IN)));
skip_totals:
			tunala_world_del_item(&world, loop);
			if(out_conns)
				fprintf(stderr, "Info, tunnel closed, down to %d\n",
					world.tunnels_used);
		}
		else {
			/* Move to the next item */
			loop++;
			t_item++;
		}
	}
	goto main_loop;
	/* Should never get here */
shouldnt_happen:
	abort();
	return 1;
}

/****************/
/* OpenSSL bits */
/****************/

static int ctx_set_cert(SSL_CTX *ctx, const char *cert, const char *key)
{
	FILE *fp = NULL;
	X509 *x509 = NULL;
	EVP_PKEY *pkey = NULL;
	int toret = 0; /* Assume an error */

	/* cert */
	if(cert) {
		if((fp = fopen(cert, "r")) == NULL) {
			fprintf(stderr, "Error opening cert file '%s'\n", cert);
			goto err;
		}
		if(!PEM_read_X509(fp, &x509, NULL, NULL)) {
			fprintf(stderr, "Error reading PEM cert from '%s'\n",
					cert);
			goto err;
		}
		if(!SSL_CTX_use_certificate(ctx, x509)) {
			fprintf(stderr, "Error, cert in '%s' can not be used\n",
					cert);
			goto err;
		}
		/* Clear the FILE* for reuse in the "key" code */
		fclose(fp);
		fp = NULL;
		fprintf(stderr, "Info, operating with cert in '%s'\n", cert);
		/* If a cert was given without matching key, we assume the same
		 * file contains the required key. */
		if(!key)
			key = cert;
	} else {
		if(key)
			fprintf(stderr, "Error, can't specify a key without a "
					"corresponding certificate\n");
		else
			fprintf(stderr, "Error, ctx_set_cert called with "
					"NULLs!\n");
		goto err;
	}
	/* key */
	if(key) {
		if((fp = fopen(key, "r")) == NULL) {
			fprintf(stderr, "Error opening key file '%s'\n", key);
			goto err;
		}
		if(!PEM_read_PrivateKey(fp, &pkey, NULL, NULL)) {
			fprintf(stderr, "Error reading PEM key from '%s'\n",
					key);
			goto err;
		}
		if(!SSL_CTX_use_PrivateKey(ctx, pkey)) {
			fprintf(stderr, "Error, key in '%s' can not be used\n",
					key);
			goto err;
		}
		fprintf(stderr, "Info, operating with key in '%s'\n", key);
	} else
		fprintf(stderr, "Info, operating without a cert or key\n");
	/* Success */
	toret = 1; err:
	if(x509)
		X509_free(x509);
	if(pkey)
		EVP_PKEY_free(pkey);
	if(fp)
		fclose(fp);
	return toret;
}

static int ctx_set_dh(SSL_CTX *ctx, const char *dh_file, const char *dh_special)
{
	DH *dh = NULL;
	FILE *fp = NULL;

	if(dh_special) {
		if(strcmp(dh_special, "NULL") == 0)
			return 1;
		if(strcmp(dh_special, "standard") == 0) {
			if((dh = get_dh512()) == NULL) {
				fprintf(stderr, "Error, can't parse 'standard'"
						" DH parameters\n");
				return 0;
			}
			fprintf(stderr, "Info, using 'standard' DH parameters\n");
			goto do_it;
		}
		if(strcmp(dh_special, "generate") != 0)
			/* This shouldn't happen - screening values is handled
			 * in main(). */
			abort();
		fprintf(stderr, "Info, generating DH parameters ... ");
		fflush(stderr);
		if((dh = DH_generate_parameters(512, DH_GENERATOR_5,
					NULL, NULL)) == NULL) {
			fprintf(stderr, "error!\n");
			return 0;
		}
		fprintf(stderr, "complete\n");
		goto do_it;
	}
	/* So, we're loading dh_file */
	if((fp = fopen(dh_file, "r")) == NULL) {
		fprintf(stderr, "Error, couldn't open '%s' for DH parameters\n",
				dh_file);
		return 0;
	}
	dh = PEM_read_DHparams(fp, NULL, NULL, NULL);
	fclose(fp);
	if(dh == NULL) {
		fprintf(stderr, "Error, could not parse DH parameters from '%s'\n",
				dh_file);
		return 0;
	}
	fprintf(stderr, "Info, using DH parameters from file '%s'\n", dh_file);
do_it:
	SSL_CTX_set_tmp_dh(ctx, dh);
	DH_free(dh);
	return 1;
}

static SSL_CTX *initialise_ssl_ctx(int server_mode, const char *engine_id,
		const char *CAfile, const char *cert, const char *key,
		const char *dcert, const char *dkey, const char *cipher_list,
		const char *dh_file, const char *dh_special, int tmp_rsa,
		int ctx_options, int out_state, int out_verify, int verify_mode,
		unsigned int verify_depth)
{
	SSL_CTX *ctx = NULL, *ret = NULL;
	SSL_METHOD *meth;
	ENGINE *e = NULL;

        OpenSSL_add_ssl_algorithms();
        SSL_load_error_strings();

	meth = (server_mode ? SSLv23_server_method() : SSLv23_client_method());
	if(meth == NULL)
		goto err;
	if(engine_id) {
		ENGINE_load_builtin_engines();
		if((e = ENGINE_by_id(engine_id)) == NULL) {
			fprintf(stderr, "Error obtaining '%s' engine, openssl "
					"errors follow\n", engine_id);
			goto err;
		}
		if(!ENGINE_set_default(e, ENGINE_METHOD_ALL)) {
			fprintf(stderr, "Error assigning '%s' engine, openssl "
					"errors follow\n", engine_id);
			goto err;
		}
		ENGINE_free(e);
	}
	if((ctx = SSL_CTX_new(meth)) == NULL)
		goto err;
	/* cacert */
	if(CAfile) {
		if(!X509_STORE_load_locations(SSL_CTX_get_cert_store(ctx),
					CAfile, NULL)) {
			fprintf(stderr, "Error loading CA cert(s) in '%s'\n",
					CAfile);
			goto err;
		}
		fprintf(stderr, "Info, operating with CA cert(s) in '%s'\n",
				CAfile);
	} else
		fprintf(stderr, "Info, operating without a CA cert(-list)\n");
	if(!SSL_CTX_set_default_verify_paths(ctx)) {
		fprintf(stderr, "Error setting default verify paths\n");
		goto err;
	}

	/* cert and key */
	if((cert || key) && !ctx_set_cert(ctx, cert, key))
		goto err;
	/* dcert and dkey */
	if((dcert || dkey) && !ctx_set_cert(ctx, dcert, dkey))
		goto err;
	/* temporary RSA key generation */
	if(tmp_rsa)
		SSL_CTX_set_tmp_rsa_callback(ctx, cb_generate_tmp_rsa);

	/* cipher_list */
	if(cipher_list) {
		if(!SSL_CTX_set_cipher_list(ctx, cipher_list)) {
			fprintf(stderr, "Error setting cipher list '%s'\n",
					cipher_list);
			goto err;
		}
		fprintf(stderr, "Info, set cipher list '%s'\n", cipher_list);
	} else
		fprintf(stderr, "Info, operating with default cipher list\n");

	/* dh_file & dh_special */
	if((dh_file || dh_special) && !ctx_set_dh(ctx, dh_file, dh_special))
		goto err;

	/* ctx_options */
	SSL_CTX_set_options(ctx, ctx_options);

	/* out_state (output of SSL handshake states to screen). */
	if(out_state)
		cb_ssl_info_set_output(stderr);

	/* out_verify */
	if(out_verify > 0) {
		cb_ssl_verify_set_output(stderr);
		cb_ssl_verify_set_level(out_verify);
	}

	/* verify_depth */
	cb_ssl_verify_set_depth(verify_depth);

	/* Success! (includes setting verify_mode) */
	SSL_CTX_set_info_callback(ctx, cb_ssl_info);
	SSL_CTX_set_verify(ctx, verify_mode, cb_ssl_verify);
	ret = ctx;
err:
	if(!ret) {
		ERR_print_errors_fp(stderr);
		if(ctx)
			SSL_CTX_free(ctx);
	}
	return ret;
}

/*****************/
/* Selector bits */
/*****************/

static void selector_sets_init(select_sets_t *s)
{
	s->max = 0;
	FD_ZERO(&s->reads);
	FD_ZERO(&s->sends);
	FD_ZERO(&s->excepts);
}
static void selector_init(tunala_selector_t *selector)
{
	selector_sets_init(&selector->last_selected);
	selector_sets_init(&selector->next_select);
}

#define SEL_EXCEPTS 0x00
#define SEL_READS   0x01
#define SEL_SENDS   0x02
static void selector_add_raw_fd(tunala_selector_t *s, int fd, int flags)
{
	FD_SET(fd, &s->next_select.excepts);
	if(flags & SEL_READS)
		FD_SET(fd, &s->next_select.reads);
	if(flags & SEL_SENDS)
		FD_SET(fd, &s->next_select.sends);
	/* Adjust "max" */
	if(s->next_select.max < (fd + 1))
		s->next_select.max = fd + 1;
}

static void selector_add_listener(tunala_selector_t *selector, int fd)
{
	selector_add_raw_fd(selector, fd, SEL_READS);
}

static void selector_add_tunala(tunala_selector_t *s, tunala_item_t *t)
{
	/* Set clean read if sm.clean_in is not full */
	if(t->clean_read != -1) {
		selector_add_raw_fd(s, t->clean_read,
			(buffer_full(state_machine_get_buffer(&t->sm,
				SM_CLEAN_IN)) ? SEL_EXCEPTS : SEL_READS));
	}
	/* Set clean send if sm.clean_out is not empty */
	if(t->clean_send != -1) {
		selector_add_raw_fd(s, t->clean_send,
			(buffer_empty(state_machine_get_buffer(&t->sm,
				SM_CLEAN_OUT)) ? SEL_EXCEPTS : SEL_SENDS));
	}
	/* Set dirty read if sm.dirty_in is not full */
	if(t->dirty_read != -1) {
		selector_add_raw_fd(s, t->dirty_read,
			(buffer_full(state_machine_get_buffer(&t->sm,
				SM_DIRTY_IN)) ? SEL_EXCEPTS : SEL_READS));
	}
	/* Set dirty send if sm.dirty_out is not empty */
	if(t->dirty_send != -1) {
		selector_add_raw_fd(s, t->dirty_send,
			(buffer_empty(state_machine_get_buffer(&t->sm,
				SM_DIRTY_OUT)) ? SEL_EXCEPTS : SEL_SENDS));
	}
}

static int selector_select(tunala_selector_t *selector)
{
	memcpy(&selector->last_selected, &selector->next_select,
			sizeof(select_sets_t));
	selector_sets_init(&selector->next_select);
	return select(selector->last_selected.max,
			&selector->last_selected.reads,
			&selector->last_selected.sends,
			&selector->last_selected.excepts, NULL);
}

/* This returns -1 for error, 0 for no new connections, or 1 for success, in
 * which case *newfd is populated. */
static int selector_get_listener(tunala_selector_t *selector, int fd, int *newfd)
{
	if(FD_ISSET(fd, &selector->last_selected.excepts))
		return -1;
	if(!FD_ISSET(fd, &selector->last_selected.reads))
		return 0;
	if((*newfd = ip_accept_connection(fd)) == -1)
		return -1;
	return 1;
}

/************************/
/* "Tunala" world stuff */
/************************/

static int tunala_world_make_room(tunala_world_t *world)
{
	unsigned int newsize;
	tunala_item_t *newarray;

	if(world->tunnels_used < world->tunnels_size)
		return 1;
	newsize = (world->tunnels_size == 0 ? 16 :
			((world->tunnels_size * 3) / 2));
	if((newarray = malloc(newsize * sizeof(tunala_item_t))) == NULL)
		return 0;
	memset(newarray, 0, newsize * sizeof(tunala_item_t));
	if(world->tunnels_used > 0)
		memcpy(newarray, world->tunnels,
			world->tunnels_used * sizeof(tunala_item_t));
	if(world->tunnels_size > 0)
		free(world->tunnels);
	/* migrate */
	world->tunnels = newarray;
	world->tunnels_size = newsize;
	return 1;
}

static int tunala_world_new_item(tunala_world_t *world, int fd,
		const char *ip, unsigned short port, int flipped)
{
	tunala_item_t *item;
	int newfd;
	SSL *new_ssl = NULL;

	if(!tunala_world_make_room(world))
		return 0;
	if((new_ssl = SSL_new(world->ssl_ctx)) == NULL) {
		fprintf(stderr, "Error creating new SSL\n");
		ERR_print_errors_fp(stderr);
		return 0;
	}
	item = world->tunnels + (world->tunnels_used++);
	state_machine_init(&item->sm);
	item->clean_read = item->clean_send =
		item->dirty_read = item->dirty_send = -1;
	if((newfd = ip_create_connection_split(ip, port)) == -1)
		goto err;
	/* Which way round? If we're a server, "fd" is the dirty side and the
	 * connection we open is the clean one. For a client, it's the other way
	 * around. Unless, of course, we're "flipped" in which case everything
	 * gets reversed. :-) */
	if((world->server_mode && !flipped) ||
			(!world->server_mode && flipped)) {
		item->dirty_read = item->dirty_send = fd;
		item->clean_read = item->clean_send = newfd;
	} else {
		item->clean_read = item->clean_send = fd;
		item->dirty_read = item->dirty_send = newfd;
	}
	/* We use the SSL's "app_data" to indicate a call-back induced "kill" */
	SSL_set_app_data(new_ssl, NULL);
	if(!state_machine_set_SSL(&item->sm, new_ssl, world->server_mode))
		goto err;
	return 1;
err:
	tunala_world_del_item(world, world->tunnels_used - 1);
	return 0;

}

static void tunala_world_del_item(tunala_world_t *world, unsigned int idx)
{
	tunala_item_t *item = world->tunnels + idx;
	if(item->clean_read != -1)
		close(item->clean_read);
	if(item->clean_send != item->clean_read)
		close(item->clean_send);
	item->clean_read = item->clean_send = -1;
	if(item->dirty_read != -1)
		close(item->dirty_read);
	if(item->dirty_send != item->dirty_read)
		close(item->dirty_send);
	item->dirty_read = item->dirty_send = -1;
	state_machine_close(&item->sm);
	/* OK, now we fix the item array */
	if(idx + 1 < world->tunnels_used)
		/* We need to scroll entries to the left */
		memmove(world->tunnels + idx,
				world->tunnels + (idx + 1),
				(world->tunnels_used - (idx + 1)) *
					sizeof(tunala_item_t));
	world->tunnels_used--;
}

static int tunala_item_io(tunala_selector_t *selector, tunala_item_t *item)
{
	int c_r, c_s, d_r, d_s; /* Four boolean flags */

	/* Take ourselves out of the gene-pool if there was an except */
	if((item->clean_read != -1) && FD_ISSET(item->clean_read,
				&selector->last_selected.excepts))
		return 0;
	if((item->clean_send != -1) && FD_ISSET(item->clean_send,
				&selector->last_selected.excepts))
		return 0;
	if((item->dirty_read != -1) && FD_ISSET(item->dirty_read,
				&selector->last_selected.excepts))
		return 0;
	if((item->dirty_send != -1) && FD_ISSET(item->dirty_send,
				&selector->last_selected.excepts))
		return 0;
	/* Grab our 4 IO flags */
	c_r = c_s = d_r = d_s = 0;
	if(item->clean_read != -1)
		c_r = FD_ISSET(item->clean_read, &selector->last_selected.reads);
	if(item->clean_send != -1)
		c_s = FD_ISSET(item->clean_send, &selector->last_selected.sends);
	if(item->dirty_read != -1)
		d_r = FD_ISSET(item->dirty_read, &selector->last_selected.reads);
	if(item->dirty_send != -1)
		d_s = FD_ISSET(item->dirty_send, &selector->last_selected.sends);
	/* If no IO has happened for us, skip needless data looping */
	if(!c_r && !c_s && !d_r && !d_s)
		return 1;
	if(c_r)
		c_r = (buffer_from_fd(state_machine_get_buffer(&item->sm,
				SM_CLEAN_IN), item->clean_read) <= 0);
	if(c_s)
		c_s = (buffer_to_fd(state_machine_get_buffer(&item->sm,
				SM_CLEAN_OUT), item->clean_send) <= 0);
	if(d_r)
		d_r = (buffer_from_fd(state_machine_get_buffer(&item->sm,
				SM_DIRTY_IN), item->dirty_read) <= 0);
	if(d_s)
		d_s = (buffer_to_fd(state_machine_get_buffer(&item->sm,
				SM_DIRTY_OUT), item->dirty_send) <= 0);
	/* If any of the flags is non-zero, that means they need closing */
	if(c_r) {
		close(item->clean_read);
		if(item->clean_send == item->clean_read)
			item->clean_send = -1;
		item->clean_read = -1;
	}
	if(c_s && (item->clean_send != -1)) {
		close(item->clean_send);
		if(item->clean_send == item->clean_read)
			item->clean_read = -1;
		item->clean_send = -1;
	}
	if(d_r) {
		close(item->dirty_read);
		if(item->dirty_send == item->dirty_read)
			item->dirty_send = -1;
		item->dirty_read = -1;
	}
	if(d_s && (item->dirty_send != -1)) {
		close(item->dirty_send);
		if(item->dirty_send == item->dirty_read)
			item->dirty_read = -1;
		item->dirty_send = -1;
	}
	/* This function name is attributed to the term donated by David
	 * Schwartz on openssl-dev, message-ID:
	 * <NCBBLIEPOCNJOAEKBEAKEEDGLIAA.davids@webmaster.com>. :-) */
	if(!state_machine_churn(&item->sm))
		/* If the SSL closes, it will also zero-out the _in buffers
		 * and will in future process just outgoing data. As and
		 * when the outgoing data has gone, it will return zero
		 * here to tell us to bail out. */
		return 0;
	/* Otherwise, we return zero if both sides are dead. */
	if(((item->clean_read == -1) || (item->clean_send == -1)) &&
			((item->dirty_read == -1) || (item->dirty_send == -1)))
		return 0;
	/* If only one side closed, notify the SSL of this so it can take
	 * appropriate action. */
	if((item->clean_read == -1) || (item->clean_send == -1)) {
		if(!state_machine_close_clean(&item->sm))
			return 0;
	}
	if((item->dirty_read == -1) || (item->dirty_send == -1)) {
		if(!state_machine_close_dirty(&item->sm))
			return 0;
	}
	return 1;
}

