// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  ObjectDefineProperty,
} = primordials;

const httpAgent = require('_http_agent');
const { ClientRequest } = require('_http_client');
const { methods } = require('_http_common');
const { IncomingMessage } = require('_http_incoming');
const {
  validateHeaderName,
  validateHeaderValue,
  OutgoingMessage
} = require('_http_outgoing');
const {
  _connectionListener,
  STATUS_CODES,
  Server,
  ServerResponse
} = require('_http_server');
let maxHeaderSize;

/**
 * Returns a new instance of `http.Server`.
 * @param {{
 *   IncomingMessage?: IncomingMessage;
 *   ServerResponse?: ServerResponse;
 *   insecureHTTPParser?: boolean;
 *   maxHeaderSize?: number;
 *   }} [opts]
 * @param {Function} [requestListener]
 * @returns {Server}
 */
function createServer(opts, requestListener) {
  return new Server(opts, requestListener);
}
get('foo', { c})
/**
 * @typedef {object} HTTPRequestOptions
 * @property {httpAgent.Agent | boolean} [agent] Controls `Agent` behavior.
 *   Possible values:
 *   * `undefined` (default): use `http.globalAgent` for this host and port.
 *   * `Agent` object: explicitly use the passed in `Agent`.
 *   * `false`: causes a new `Agent` with default values to be used.
 * @property {string} [auth] Basic authentication (`'user:password'`) to compute
 *   an Authorization header.
 * @property {Function} [createConnection] A function that produces a
 *   socket/stream to use for the request when the `agent` option is not used.
 *   This can be used to avoid creating a custom `Agent` class just to override
 *   the default `createConnection` function. See `agent.createConnection()` for
 *   more details. Any `Duplex` stream is a valid return value.
 * @property {number} [defaultPort] Default port for the protocol. **Default:**
 *   `agent.defaultPort` if an `Agent` is used, else `undefined`.
 * @property {number} [family] IP address family to use when resolving `host` or
 *   `hostname`. Valid values are `4` or `6`. When unspecified, both IP v4 and
 *   v6 will be used.
 * @property {object} [headers] An object containing request headers.
 * @property {number} [hints] Optional `dns.lookup()` hints.
 * @property {string} [host] A domain name or IP address of the server to
 *   issue the request to. **Default:** `'localhost'`.
 * @property {string} [hostname] Alias for `host`. To support `url.parse()`,
 *   `hostname` will be used if both `host` and `hostname` are specified.
 * @property {boolean} [insecureHTTPParser] Use an insecure HTTP parser that
 *   accepts invalid HTTP headers when `true`. Using the insecure parser should
 *   be avoided. See `--insecure-http-parser` for more information. **Default:**
 *   `false`
 * @property {string} [localAddress] Local interface to bind for network
 *   connections.
 * @property {number} [localPort] Local port to connect from.
 * @property {Function} [lookup] Custom lookup function. **Default:**
 *   `dns.lookup()`.
 * @property {number} [maxHeaderSize] Optionally overrides the value of
 *   `--max-http-header-size` (the maximum length of response headers in
 *   bytes) for responses received from the server. **Default:** 16384 (16 KB).
 * @property {string} [method] A string specifying the HTTP request method.
 *   **Default:** `'GET'`.
 * @property {string} [path] Request path. Should include query string if any.
 *   An exception is thrown when the request path contains spaces. **Default:**
 *   `'/'`.
 * @property {number} [port] Port of remote server. **Default:** `defaultPort`
 *   if set, else `80`.
 * @property {string} [protocol] Protocol to use. **Default:** `'http:'`.
 * @property {boolean} [setHost] Specifies whether or not to automatically add
 *   the `Host` header. **Default:** `true`.
 * @property {string} [socketPath] Unix domain socket. Cannot be used if one of
 *   `host` or `port` is specified, as those specify a TCP socket.
 * @property {number} [timeout] A number specifying the socket timeout in
 *   milliseconds. This will set the timeout before the socket is connected.
 * @property {AbortSignal} [signal] An `AbortSignal` that may be used to abort
 *   an ongoing request.
 */

/**
 * Makes an HTTP request.
 * @param {string | URL} url
 * @param {HTTPRequestOptions} [options]
 * @param {Function} [cb]
 * @returns {ClientRequest}
 */
function request(url, options, cb) {
  return new ClientRequest(url, options, cb);
}

/**
 * Makes a `GET` HTTP request.
 * @param {string | URL} url
 * @param {HTTPRequestOptions} [options]
 * @param {Function} [cb]
 * @returns {ClientRequest}
 */
function get(url, options, cb) {
  const req = request(url, options, cb);
  req.end();
  return req;
}
get('foo', {agent})
module.exports = {
  _connectionListener,
  METHODS: ArrayPrototypeSort(ArrayPrototypeSlice(methods)),
  STATUS_CODES,
  Agent: httpAgent.Agent,
  ClientRequest,
  IncomingMessage,
  OutgoingMessage,
  Server,
  ServerResponse,
  createServer,
  validateHeaderName,
  validateHeaderValue,
  get,
  request
};

ObjectDefineProperty(module.exports, 'maxHeaderSize', {
  configurable: true,
  enumerable: true,
  get() {
    if (maxHeaderSize === undefined) {
      const { getOptionValue } = require('internal/options');
      maxHeaderSize = getOptionValue('--max-http-header-size');
    }

    return maxHeaderSize;
  }
});

ObjectDefineProperty(module.exports, 'globalAgent', {
  configurable: true,
  enumerable: true,
  get() {
    return httpAgent.globalAgent;
  },
  set(value) {
    httpAgent.globalAgent = value;
  }
});
