'use strict';
const {
  ArrayPrototypePush,
  Promise,
  PromisePrototypeThen,
  PromiseResolve,
  SafeMap,
  StringPrototypeEndsWith,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;
const {
  Buffer: {
    concat: BufferConcat
  }
} = require('buffer');
const {
  ERR_NETWORK_IMPORT_DISALLOWED,
  ERR_NETWORK_IMPORT_BAD_RESPONSE,
} = require('internal/errors').codes;
const { URL } = require('internal/url');
const net = require('net');

/**
 * @typedef CacheEntry
 * @property {Promise<string> | string} resolvedHREF
 * @property {Record<string, string>} headers
 * @property {Promise<Buffer> | Buffer} body
 */

/**
 * Only for GET requests, other requests would need new Map
 * HTTP cache semantics keep diff caches
 *
 * Maps HREF to pending cache entry
 * @type {Map<string, Promise<CacheEntry> | CacheEntry>}
 */
const cacheForGET = new SafeMap();

// [1] The V8 snapshot doesn't like some C++ APIs to be loaded eagerly. Do it
// lazily/at runtime and not top level of an internal module.

// [2] Creating a new agent instead of using the gloabl agent improves
// performance and precludes the agent becoming tainted.

let HTTPSAgent;
function HTTPSGet(url, opts) {
  const https = require('https'); // [1]
  HTTPSAgent ??= new https.Agent({ // [2]
    keepAlive: true
  });
  return https.get(url, {
    agent: HTTPSAgent,
    ...opts
  });
}

let HTTPAgent;
function HTTPGet(url, opts) {
  const http = require('http'); // [1]
  HTTPAgent ??= new http.Agent({ // [2]
    keepAlive: true
  });
  return http.get(url, {
    agent: HTTPAgent,
    ...opts
  });
}

function dnsLookup(name, opts) {
  // eslint-disable-next-line no-func-assign
  dnsLookup = require('dns/promises').lookup;
  return dnsLookup(name, opts);
}

let zlib;
function createBrotliDecompress() {
  zlib ??= require('zlib'); // [1]
  // eslint-disable-next-line no-func-assign
  createBrotliDecompress = zlib.createBrotliDecompress;
  return createBrotliDecompress();
}

function createUnzip() {
  zlib ??= require('zlib'); // [1]
  // eslint-disable-next-line no-func-assign
  createUnzip = zlib.createUnzip;
  return createUnzip();
}

/**
 * @param {URL} parsed
 * @returns {Promise<CacheEntry> | CacheEntry}
 */
function fetchWithRedirects(parsed) {
  const existing = cacheForGET.get(parsed.href);
  if (existing) {
    return existing;
  }
  const handler = parsed.protocol === 'http:' ? HTTPGet : HTTPSGet;
  const result = new Promise((fulfill, reject) => {
    const req = handler(parsed, {
      headers: {
        Accept: '*/*'
      }
    })
    .on('error', reject)
    .on('response', (res) => {
      function dispose() {
        req.destroy();
        res.destroy();
      }
      if (res.statusCode >= 300 && res.statusCode <= 303) {
        if (res.headers.location) {
          dispose();
          try {
            const location = new URL(res.headers.location, parsed);
            if (location.protocol !== 'http:' &&
              location.protocol !== 'https:') {
              reject(new ERR_NETWORK_IMPORT_DISALLOWED(
                res.headers.location,
                parsed.href,
                'cannot redirect to non-network location'));
              return;
            }
            return PromisePrototypeThen(
              PromiseResolve(fetchWithRedirects(location)),
              (entry) => {
                cacheForGET.set(parsed.href, entry);
                fulfill(entry);
              });
          } catch (e) {
            dispose();
            reject(e);
          }
        }
      }
      if (res.statusCode > 303 || res.statusCode < 200) {
        dispose();
        reject(
          new ERR_NETWORK_IMPORT_BAD_RESPONSE(
            parsed.href,
            'HTTP response returned status code of ' + res.statusCode));
        return;
      }
      const { headers } = res;
      const contentType = headers['content-type'];
      if (!contentType) {
        dispose();
        reject(new ERR_NETWORK_IMPORT_BAD_RESPONSE(
          parsed.href,
          'the \'Content-Type\' header is required'));
        return;
      }
      /**
       * @type {CacheEntry}
       */
      const entry = {
        resolvedHREF: parsed.href,
        headers: {
          'content-type': res.headers['content-type']
        },
        body: new Promise((f, r) => {
          const buffers = [];
          let size = 0;
          let bodyStream = res;
          let onError;
          if (res.headers['content-encoding'] === 'br') {
            bodyStream = createBrotliDecompress();
            onError = function onError(error) {
              bodyStream.close();
              dispose();
              reject(error);
              r(error);
            };
            res.on('error', onError);
            res.pipe(bodyStream);
          } else if (res.headers['content-encoding'] === 'gzip' ||
            res.headers['content-encoding'] === 'deflate') {
            bodyStream = createUnzip();
            onError = function onError(error) {
              bodyStream.close();
              dispose();
              reject(error);
              r(error);
            };
            res.on('error', onError);
            res.pipe(bodyStream);
          } else {
            onError = function onError(error) {
              dispose();
              reject(error);
              r(error);
            };
          }
          bodyStream.on('error', onError);
          bodyStream.on('data', (d) => {
            ArrayPrototypePush(buffers, d);
            size += d.length;
          });
          bodyStream.on('end', () => {
            const body = entry.body = /** @type {Buffer} */(
              BufferConcat(buffers, size)
            );
            f(body);
          });
        }),
      };
      cacheForGET.set(parsed.href, entry);
      fulfill(entry);
    });
  });
  cacheForGET.set(parsed.href, result);
  return result;
}

const allowList = new net.BlockList();
allowList.addAddress('::1', 'ipv6');
allowList.addRange('127.0.0.1', '127.255.255.255');

/**
 * Returns if an address has local status by if it is going to a local
 * interface or is an address resolved by DNS to be a local interface
 * @param {string} hostname url.hostname to test
 * @returns {Promise<boolean>}
 */
async function isLocalAddress(hostname) {
  try {
    if (StringPrototypeStartsWith(hostname, '[') &&
        StringPrototypeEndsWith(hostname, ']')) {
      hostname = StringPrototypeSlice(hostname, 1, -1);
    }
    const addr = await dnsLookup(hostname, { verbatim: true });
    const ipv = addr.family === 4 ? 'ipv4' : 'ipv6';
    return allowList.check(addr.address, ipv);
  } catch {
    // If it errored, the answer is no.
  }
  return false;
}

/**
 * Fetches a location with a shared cache following redirects.
 * Does not respect HTTP cache headers.
 *
 * This splits the header and body Promises so that things only needing
 * headers don't need to wait on the body.
 *
 * In cases where the request & response have already settled, this returns the
 * cache value synchronously.
 *
 * @param {URL} parsed
 * @param {ESModuleContext} context
 * @returns {ReturnType<typeof fetchWithRedirects>}
 */
function fetchModule(parsed, { parentURL }) {
  const { href } = parsed;
  const existing = cacheForGET.get(href);
  if (existing) {
    return existing;
  }
  if (parsed.protocol === 'http:') {
    return PromisePrototypeThen(isLocalAddress(parsed.hostname), (is) => {
      if (is !== true) {
        throw new ERR_NETWORK_IMPORT_DISALLOWED(
          href,
          parentURL,
          'http can only be used to load local resources (use https instead).'
        );
      }
      return fetchWithRedirects(parsed);
    });
  }
  return fetchWithRedirects(parsed);
}

module.exports = {
  fetchModule: fetchModule
};
