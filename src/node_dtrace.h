#ifndef NODE_DTRACE_H_
#define NODE_DTRACE_H_

#include <node.h>
#include <v8.h>

extern "C" {

typedef struct {
	int32_t fd;
	int32_t port;
	char *remote;
} node_dtrace_connection_t;

typedef struct {
	char *url;
	char *method;
} node_dtrace_http_request_t;

}

namespace node {

void InitDTrace(v8::Handle<v8::Object> target);

}

#endif
