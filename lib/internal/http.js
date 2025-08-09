'use strict';

const {
  Date,
  NumberParseInt,
  Symbol,
  decodeURIComponent,
} = primordials;

const { setUnrefTimeout } = require('internal/timers');
const { getCategoryEnabledBuffer, trace } = internalBinding('trace_events');
const {
  CHAR_LOWERCASE_B,
  CHAR_LOWERCASE_E,
} = require('internal/constants');

const { URL } = require('internal/url');
const { Buffer } = require('buffer');
const { isIPv4 } = require('internal/net');
const { ERR_PROXY_INVALID_CONFIG } = require('internal/errors').codes;
let utcCache;

function utcDate() {
  if (!utcCache) cache();
  return utcCache;
}

function cache() {
  const d = new Date();
  utcCache = d.toUTCString();
  setUnrefTimeout(resetCache, 1000 - d.getMilliseconds());
}

function resetCache() {
  utcCache = undefined;
}

let traceEventId = 0;

function getNextTraceEventId() {
  return ++traceEventId;
}

const httpEnabled = getCategoryEnabledBuffer('node.http');

function isTraceHTTPEnabled() {
  return httpEnabled[0] > 0;
}

const traceEventCategory = 'node,node.http';

function traceBegin(...args) {
  trace(CHAR_LOWERCASE_B, traceEventCategory, ...args);
}

function traceEnd(...args) {
  trace(CHAR_LOWERCASE_E, traceEventCategory, ...args);
}

function ipToInt(ip) {
  let result = 0;
  let multiplier = 1;
  let octetShift = 0;
  let code = 0;

  for (let i = ip.length - 1; i >= 0; --i) {
    code = ip.charCodeAt(i);
    if (code !== 46) {
      result += ((code - 48) * multiplier) << octetShift;
      multiplier *= 10;
    } else {
      octetShift += 8;
      multiplier = 1;
    }
  }
  return result >>> 0;
}

// There are two factors in play when proxying the request:
// 1. What the request protocol is, that is, whether users are sending it via
//    http.request or https.request, or whether they are sending
//    the request to a https:// URL or a http:// URL. HTTPS requests should be
//    proxied by the proxy specified using the HTTPS_PROXY environment variable.
//    HTTP requests should be proxied by the proxy specified using the HTTP_PROXY
//    environment variable.
// 2. What the proxy protocol is. This depends on the value of the environment variables,
//    for example.
//
// When proxying a HTTP request, the following needs to be done:
// https://datatracker.ietf.org/doc/html/rfc7230#section-5.3.2
// 1. Rewrite the request path to absolute-form.
// 2. Add proxy-connection and proxy-authorization headers appropriately.
//
// When proxying a HTTPS request, the following needs to be done:
// https://datatracker.ietf.org/doc/html/rfc9110#CONNECT
// 1. Send a CONNECT request to the proxy server.
// 2. Wait for 200 connection established response to establish the tunnel.
// 3. Perform TLS handshake with the endpoint through the tunnel.
// 4. Tunnel the request using the established connection.
//
// When the proxy protocol is HTTP, the modified HTTP request can just be sent over
// the TCP socket to the proxy server, and the HTTPS request tunnel can be established
// over the TCP socket to the proxy server.
// When the proxy protocol is HTTPS, the modified request needs to be sent after
// TLS handshake with the proxy server. Same goes to the HTTPS request tunnel establishment.

/**
 * @callback ProxyBypassMatchFn
 * @param {string} host - Host to match against the bypass list.
 * @param {string} [hostWithPort] - Host with port to match against the bypass list.
 * @returns {boolean} - True if the host should be bypassed, false otherwise.
 */

/**
 * @typedef {object} ProxyConnectionOptions
 * @property {string} host - Hostname of the proxy server.
 * @property {number} port - Port of the proxy server.
 */

/**
 * Represents the proxy configuration for an agent. The built-in http and https agent
 * implementation have one of this when they are configured to use a proxy.
 * @property {string} href - Full URL of the proxy server.
 * @property {string} host - Full host including port, e.g. 'localhost:8080'.
 * @property {string} hostname - Hostname without brackets for IPv6 addresses.
 * @property {number} port - Port number of the proxy server.
 * @property {string} protocol - Protocol of the proxy server, e.g. 'http:' or 'https:'.
 * @property {string|undefined} auth - proxy-authorization header value, if username or password is provided.
 * @property {Array<string>} bypassList - List of hosts to bypass the proxy.
 * @property {ProxyConnectionOptions} proxyConnectionOptions - Options for connecting to the proxy server.
 */
class ProxyConfig {
  /** @type {Array<string>} */
  #bypassList = [];
  /** @type {Array<ProxyBypassMatchFn>} */
  #bypassMatchFns = [];

  /** @type {ProxyConnectionOptions} */
  get proxyConnectionOptions() {
    return {
      host: this.hostname,
      port: this.port,
    };
  }

  /**
   * @param {string} proxyUrl - The URL of the proxy server, e.g. 'http://localhost:8080'.
   * @param {boolean} [keepAlive] - Whether to keep the connection alive.
   *   This is not used in the current implementation but can be used in the future.
   * @param {string} [noProxyList] - Comma-separated list of hosts to bypass the proxy.
   */
  constructor(proxyUrl, keepAlive, noProxyList) {
    const { host, hostname, port, protocol, username, password } = new URL(proxyUrl);
    this.href = proxyUrl; // Full URL of the proxy server.
    this.host = host; // Full host including port, e.g. 'localhost:8080'.
    this.hostname = hostname.replace(/^\[|\]$/g, ''); // Trim off the brackets from IPv6 addresses.
    this.port = port ? NumberParseInt(port, 10) : (protocol === 'https:' ? 443 : 80);
    this.protocol = protocol; // Protocol of the proxy server, e.g. 'http:' or 'https:'.

    if (username || password) {
      // If username or password is provided, prepare the proxy-authorization header.
      const auth = `${decodeURIComponent(username)}:${decodeURIComponent(password)}`;
      this.auth = `Basic ${Buffer.from(auth).toString('base64')}`;
    }

    if (noProxyList) {
      this.#bypassList = noProxyList
        .split(',')
        .map((entry) => entry.trim().toLowerCase());
    }

    if (this.#bypassList.length === 0) {
      this.shouldUseProxy = () => true; // No bypass list, always use the proxy.
    } else if (this.#bypassList.includes('*')) {
      this.shouldUseProxy = () => false; // '*' in the bypass list means to bypass all hosts.
    } else {
      this.#buildBypassMatchFns();
      // Use the bypass match functions to determine if the proxy should be used.
      this.shouldUseProxy = this.#match.bind(this);
    }
  }

  #buildBypassMatchFns(bypassList = this.#bypassList) {
    this.#bypassMatchFns = [];

    for (const entry of this.#bypassList) {
      if (
        // Handle wildcard entries like *.example.com
        entry.startsWith('*.') ||
        // Follow curl's behavior: strip leading dot before matching suffixes.
        entry.startsWith('.')
      ) {
        const suffix = entry.split('');
        suffix.shift(); // Remove the leading dot or asterisk.
        const suffixLength = suffix.length;
        if (suffixLength === 0) {
          // If the suffix is empty, it means to match all hosts.
          this.#bypassMatchFns.push(() => true);
          continue;
        }
        this.#bypassMatchFns.push((host) => {
          const hostLength = host.length;
          const offset = hostLength - suffixLength;
          if (offset < 0) return false; // Host is shorter than the suffix.
          for (let i = 0; i < suffixLength; i++) {
            if (host[offset + i] !== suffix[i]) {
              return false;
            }
          }
          return true;
        });
        continue;
      }

      // Handle IP ranges (simple format like 192.168.1.0-192.168.1.255)
      // TODO(joyeecheung): support IPv6.
      const { 0: startIP, 1: endIP } = entry.split('-').map((ip) => ip.trim());
      if (entry.includes('-') && startIP && endIP && isIPv4(startIP) && isIPv4(endIP)) {
        const startInt = ipToInt(startIP);
        const endInt = ipToInt(endIP);
        this.#bypassMatchFns.push((host) => {
          if (isIPv4(host)) {
            const hostInt = ipToInt(host);
            return hostInt >= startInt && hostInt <= endInt;
          }
          return false;
        });
        continue;
      }

      // Handle simple host or IP entries
      this.#bypassMatchFns.push((host, hostWithPort) => {
        return (host === entry || hostWithPort === entry);
      });
    }
  }

  get bypassList() {
    // Return a copy of the bypass list to prevent external modification.
    return [...this.#bypassList];
  }

  // See: https://about.gitlab.com/blog/we-need-to-talk-no-proxy
  // TODO(joyeecheung): share code with undici.
  #match(hostname, port) {
    const host = hostname.toLowerCase();
    const hostWithPort = port ? `${host}:${port}` : host;

    for (const bypassMatchFn of this.#bypassMatchFns) {
      if (bypassMatchFn(host, hostWithPort)) {
        return false; // If any bypass function matches, do not use the proxy.
      }
    }

    return true; // If no matches found, use the proxy.
  }
}

function parseProxyConfigFromEnv(env, protocol, keepAlive) {
  // We only support proxying for HTTP and HTTPS requests.
  if (protocol !== 'http:' && protocol !== 'https:') {
    return null;
  }
  // Get the proxy url - following the most popular convention, lower case takes precedence.
  // See https://about.gitlab.com/blog/we-need-to-talk-no-proxy/#http_proxy-and-https_proxy
  const proxyUrl = (protocol === 'https:') ?
    (env.https_proxy || env.HTTPS_PROXY) : (env.http_proxy || env.HTTP_PROXY);
  // No proxy settings from the environment, ignore.
  if (!proxyUrl) {
    return null;
  }

  if (proxyUrl.includes('\r') || proxyUrl.includes('\n')) {
    throw new ERR_PROXY_INVALID_CONFIG(`Invalid proxy URL: ${proxyUrl}`);
  }

  // Only http:// and https:// proxies are supported.
  // Ignore instead of throw, in case other protocols are supposed to be
  // handled by the user land.
  if (!proxyUrl.startsWith('http://') && !proxyUrl.startsWith('https://')) {
    return null;
  }

  const noProxyList = env.no_proxy || env.NO_PROXY;
  return new ProxyConfig(proxyUrl, keepAlive, noProxyList);
}

/**
 * @param {ProxyConfig} proxyConfig
 * @param {object} reqOptions
 * @returns {boolean}
 */
function checkShouldUseProxy(proxyConfig, reqOptions) {
  if (!proxyConfig) {
    return false;
  }
  if (reqOptions.socketPath) {
    // If socketPath is set, the endpoint is a Unix domain socket, which can't
    // be proxied.
    return false;
  }
  return proxyConfig.shouldUseProxy(reqOptions.host || 'localhost', reqOptions.port);
}

function filterEnvForProxies(env) {
  return {
    __proto__: null,
    http_proxy: env.http_proxy,
    HTTP_PROXY: env.HTTP_PROXY,
    https_proxy: env.https_proxy,
    HTTPS_PROXY: env.HTTPS_PROXY,
    no_proxy: env.no_proxy,
    NO_PROXY: env.NO_PROXY,
  };
}

module.exports = {
  kOutHeaders: Symbol('kOutHeaders'),
  kNeedDrain: Symbol('kNeedDrain'),
  kProxyConfig: Symbol('kProxyConfig'),
  kWaitForProxyTunnel: Symbol('kWaitForProxyTunnel'),
  checkShouldUseProxy,
  parseProxyConfigFromEnv,
  utcDate,
  traceBegin,
  traceEnd,
  getNextTraceEventId,
  isTraceHTTPEnabled,
  filterEnvForProxies,
};
