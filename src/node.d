/* Copyright Joyent, Inc. and other Node contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This is the DTrace library file for the node provider, which includes
 * the necessary translators to get from the args[] to something useful.
 * Be warned:  the mechanics here are seriously ugly -- and one must always
 * keep in mind that clean abstractions often require filthy systems.
 */
#pragma D depends_on library procfs.d

typedef struct {
	int32_t fd;
	int32_t port;
	uint32_t remote;
	uint32_t buffered;
} node_dtrace_connection_t;

typedef struct {
	int32_t fd;
	int32_t port;
	uint64_t remote;
	uint32_t buffered;
} node_dtrace_connection64_t;

typedef struct {
	int fd;
	string remoteAddress;
	int remotePort;
	int bufferSize;
} node_connection_t;

translator node_connection_t <node_dtrace_connection_t *nc> {
	fd = *(int32_t *)copyin((uintptr_t)&nc->fd, sizeof (int32_t));
	remotePort =
	    *(int32_t *)copyin((uintptr_t)&nc->port, sizeof (int32_t));
	remoteAddress = curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
	    copyinstr((uintptr_t)*(uint32_t *)copyin((uintptr_t)&nc->remote,
	    sizeof (int32_t))) :
	    copyinstr((uintptr_t)*(uint64_t *)copyin((uintptr_t)
	    &((node_dtrace_connection64_t *)nc)->remote, sizeof (int64_t)));
	bufferSize = curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
	    *(uint32_t *)copyin((uintptr_t)&nc->buffered, sizeof (int32_t)) :
	    *(uint32_t *)copyin((uintptr_t)
	    &((node_dtrace_connection64_t *)nc)->buffered, sizeof (int32_t));
};

/*
 * 32-bit and 64-bit structures received from node for HTTP client request
 * probe.
 */
typedef struct {
	uint32_t url;
	uint32_t method;
} node_dtrace_http_client_request_t;

typedef struct {
	uint64_t url;
	uint64_t method;
} node_dtrace_http_client_request64_t;

/*
 * The following structures are never used directly, but must exist to bind the
 * types specified in the provider to the translators defined here.
 * Ultimately, they always get cast to a more specific type inside the
 * translator.  To add to the confusion, the DTrace compiler does not allow
 * declaring two translators with the same destination type if the source types
 * are structures with the same size (because libctf says they're compatible,
 * so dtrace considers them equivalent).  Since we must define translators from
 * node_dtrace_http_client_request_t (above), node_dtrace_http_request_t, and
 * node_dtrace_http_server_request_t (both below), each of these three structs
 * must be declared with a different size.
 */
typedef struct {
	uint32_t version;
	uint64_t dummy1;
} node_dtrace_http_request_t;

typedef struct {
	uint32_t version;
	uint64_t dummy2;
	uint64_t dummy3;
} node_dtrace_http_server_request_t;

/*
 * Actual 32-bit and 64-bit, v0 and v1 structures received from node for the
 * HTTP server request probe.
 */
typedef struct {
	uint32_t url;
	uint32_t method;
} node_dtrace_http_server_request_v0_t;

typedef struct {
	uint32_t version;
	uint32_t url;
	uint32_t method;
	uint32_t forwardedFor;
} node_dtrace_http_server_request_v1_t;

typedef struct {
	uint64_t url;
	uint64_t method;
} node_dtrace_http_server_request64_v0_t;

typedef struct {
	uint32_t version;
	uint32_t pad;
	uint64_t url;
	uint64_t method;
	uint64_t forwardedFor;
} node_dtrace_http_server_request64_v1_t;

/*
 * In the end, both client and server request probes from both old and new
 * binaries translate their arguments to node_http_request_t, which is what the
 * user's D script ultimately sees.
 */
typedef struct {
	string url;
	string method;
	string forwardedFor;
} node_http_request_t;

/*
 * The following translators are particularly filthy for reasons of backwards
 * compatibility.  Stable versions of node prior to 0.6 used a single
 * http_request struct with fields for "url" and "method" for both client and
 * server probes.  0.6 added a "forwardedFor" field intended for the server
 * probe only, and the http_request struct passed by the application was split
 * first into client_http_request and server_http_request and the latter was
 * again split for v0 (the old struct) and v1.
 *
 * To distinguish the binary representations of the two versions of these
 * structs, the new version prepends a "version" member (where the old one has
 * a "url" pointer).  Each field that we're translating below first switches on
 * the value of this "version" field: if it's larger than 4096, we know we must
 * be looking at the "url" pointer of the older structure version.  Otherwise,
 * we must be looking at the new version.  Besides this, we have the usual
 * switch based on the userland process data model.  This would all be simpler
 * with macros, but those aren't available in D library files since we cannot
 * rely on cpp being present at runtime.
 *
 * In retrospect, the versioning bit might have been unnecessary since the type
 * of the object passed in should allow DTrace to select which translator to
 * use.  However, DTrace does sometimes use translators whose source types
 * don't quite match, and since we know this versioning logic works, we just
 * leave it alone.  Each of the translators below is functionally identical
 * (except that the client -> client translator doesn't bother translating
 * forwardedFor) and should actually work with any version of any of the client
 * or server structs transmitted by the application up to this point.
 */

/*
 * Translate from node_dtrace_http_server_request_t (received from node 0.6 and
 * later versions) to node_http_request_t.
 */
translator node_http_request_t <node_dtrace_http_server_request_t *nd> {
	url = (*(uint32_t *)copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ?
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v0_t *)nd)->url,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v0_t *)nd)->url,
		    sizeof (uint64_t)))) :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->url,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->url,
		    sizeof (uint64_t))));

	method = (*(uint32_t *)copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ?
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v0_t *)nd)->method,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v0_t *)nd)->method,
		    sizeof (uint64_t)))) :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->method,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->method,
		    sizeof (uint64_t))));

	forwardedFor = (*(uint32_t *)copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ? "" :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->forwardedFor,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->
		    forwardedFor, sizeof (uint64_t))));
};

/*
 * Translate from node_dtrace_http_client_request_t (received from node 0.6 and
 * later versions) to node_http_request_t.
 */
translator node_http_request_t <node_dtrace_http_client_request_t *nd> {
	url = (*(uint32_t *)copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ?
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v0_t *)nd)->url,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v0_t *)nd)->url,
		    sizeof (uint64_t)))) :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->url,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->url,
		    sizeof (uint64_t))));

	method = (*(uint32_t *)copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ?
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v0_t *)nd)->method,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v0_t *)nd)->method,
		    sizeof (uint64_t)))) :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->method,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->method,
		    sizeof (uint64_t))));

	forwardedFor = "";
};

/*
 * Translate from node_dtrace_http_request_t (received from versions of node
 * prior to 0.6) to node_http_request_t.  This is used for both the server and
 * client probes since these versions of node didn't distinguish between the
 * types used in these probes.
 */
translator node_http_request_t <node_dtrace_http_request_t *nd> {
	url = (*(uint32_t *)copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ?
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v0_t *)nd)->url,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v0_t *)nd)->url,
		    sizeof (uint64_t)))) :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->url,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->url,
		    sizeof (uint64_t))));

	method = (*(uint32_t *)copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ?
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v0_t *)nd)->method,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v0_t *)nd)->method,
		    sizeof (uint64_t)))) :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->method,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->method,
		    sizeof (uint64_t))));

	forwardedFor = (*(uint32_t *) copyin((uintptr_t)(uint32_t *)nd,
		sizeof (uint32_t))) >= 4096 ? "" :
	    (curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
		copyinstr(*(uint32_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request_v1_t *)nd)->forwardedFor,
		    sizeof (uint32_t))) :
		copyinstr(*(uint64_t *)copyin((uintptr_t)
		    &((node_dtrace_http_server_request64_v1_t *)nd)->
		    forwardedFor, sizeof (uint64_t))));
};
