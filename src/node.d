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

typedef struct {
	uint32_t url;
	uint32_t method;
} node_dtrace_http_request_t;

typedef struct {
	uint64_t url;
	uint64_t method;
} node_dtrace_http_request64_t;

typedef struct {
	string url;
	string method;
} node_http_request_t;

translator node_http_request_t <node_dtrace_http_request_t *nd> {
	url = curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
	    copyinstr((uintptr_t)*(uint32_t *)copyin((uintptr_t)&nd->url,
	    sizeof (int32_t))) :
	    copyinstr((uintptr_t)*(uint64_t *)copyin((uintptr_t)
	    &((node_dtrace_http_request64_t *)nd)->url, sizeof (int64_t)));
	method = curpsinfo->pr_dmodel == PR_MODEL_ILP32 ?
	    copyinstr((uintptr_t)*(uint32_t *)copyin((uintptr_t)&nd->method,
	    sizeof (int32_t))) :
	    copyinstr((uintptr_t)*(uint64_t *)copyin((uintptr_t)
	    &((node_dtrace_http_request64_t *)nd)->method, sizeof (int64_t)));
};
