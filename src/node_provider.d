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
	probe net__socket__read(node_dtrace_connection_t *c, int b) :
	    (node_connection_t *c, int b);
	probe net__socket__write(node_dtrace_connection_t *c, int b) :
	    (node_connection_t *c, int b);
	probe http__server__request(node_dtrace_http_request_t *h,
	    node_dtrace_connection_t *c) :
	    (node_http_request_t *h, node_connection_t *c);
	probe http__server__response(node_dtrace_connection_t *c) :
	    (node_connection_t *c);
	probe http__client__request(node_dtrace_http_request_t *h,
	    node_dtrace_connection_t *c) :
	    (node_http_request_t *h, node_connection_t *c);
	probe http__client__response(node_dtrace_connection_t *c) :
	    (node_connection_t *c);
	probe gc__start(int t, int f);
	probe gc__done(int t, int f);
};

#pragma D attributes Evolving/Evolving/ISA provider node provider
#pragma D attributes Private/Private/Unknown provider node module
#pragma D attributes Private/Private/Unknown provider node function
#pragma D attributes Private/Private/ISA provider node name
#pragma D attributes Evolving/Evolving/ISA provider node args
