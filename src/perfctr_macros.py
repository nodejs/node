# This file is used by tools/js2c.py to preprocess out the performance counters
# symbols in builds that don't support counters. This is not used in builds
# that support performance counters.
macro COUNTER_NET_SERVER_CONNECTION(x) = ;
macro COUNTER_NET_SERVER_CONNECTION_CLOSE(x) = ;
macro COUNTER_HTTP_SERVER_REQUEST(x) = ;
macro COUNTER_HTTP_SERVER_RESPONSE(x) = ;
macro COUNTER_HTTP_CLIENT_REQUEST(x) = ;
macro COUNTER_HTTP_CLIENT_RESPONSE(x) = ;
