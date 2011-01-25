/*
 * DTrace provider for node.js.
 */

/*
 * In order to have the information we need here to create the provider,
 * we must declare bogus definitions for our depended-upon structures.  And
 * yes, the fact that we need to do this represents a shortcoming in DTrace,
 * one that would be resolved by that elusive El Dorado:  dynamic translators.
 */

typedef struct {
	int dummy;
} node_dtrace_connection_t;

typedef struct {
	int dummy;
} node_connection_t;

typedef struct {
	int dummy;
} node_dtrace_http_request_t;

typedef struct {
	int dummy;
} node_http_request_t;

provider node {
	probe net__server__connection(node_dtrace_connection_t *c) :
	    (node_connection_t *c);
	probe net__stream__end(node_dtrace_connection_t *c) :
	    (node_connection_t *c);
	probe http__server__request(node_dtrace_http_request_t *h,
	    node_dtrace_connection_t *c) :
	    (node_http_request_t *h, node_connection_t *c);
	probe http__server__response(node_dtrace_connection_t *c) :
	    (node_connection_t *c);
};

#pragma D attributes Evolving/Evolving/ISA provider node provider
#pragma D attributes Private/Private/Unknown provider node module
#pragma D attributes Private/Private/Unknown provider node function
#pragma D attributes Private/Private/ISA provider node name
#pragma D attributes Evolving/Evolving/ISA provider node args
