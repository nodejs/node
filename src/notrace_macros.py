# This file is used by tools/js2c.py to preprocess out the DTRACE symbols in
# builds that don't support DTrace. This is not used in builds that support
# DTrace.
macro DTRACE_HTTP_CLIENT_REQUEST(x) = ;
macro DTRACE_HTTP_CLIENT_RESPONSE(x) = ;
macro DTRACE_HTTP_SERVER_REQUEST(x) = ;
macro DTRACE_HTTP_SERVER_RESPONSE(x) = ;
macro DTRACE_NET_SERVER_CONNECTION(x) = ;
macro DTRACE_NET_STREAM_END(x) = ;
macro DTRACE_NET_SOCKET_READ(x) = ;
macro DTRACE_NET_SOCKET_WRITE(x) = ;
