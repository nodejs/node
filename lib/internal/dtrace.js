'use strict';

const config = internalBinding('config');

const {
  DTRACE_HTTP_CLIENT_REQUEST = () => {},
  DTRACE_HTTP_CLIENT_RESPONSE = () => {},
  DTRACE_HTTP_SERVER_REQUEST = () => {},
  DTRACE_HTTP_SERVER_RESPONSE = () => {},
  DTRACE_NET_SERVER_CONNECTION = () => {},
  DTRACE_NET_STREAM_END = () => {}
} = (config.hasDtrace ? internalBinding('dtrace') : {});

module.exports = {
  DTRACE_HTTP_CLIENT_REQUEST,
  DTRACE_HTTP_CLIENT_RESPONSE,
  DTRACE_HTTP_SERVER_REQUEST,
  DTRACE_HTTP_SERVER_RESPONSE,
  DTRACE_NET_SERVER_CONNECTION,
  DTRACE_NET_STREAM_END
};
