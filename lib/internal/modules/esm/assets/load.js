'use strict';
const {
  globalThis,
  ArrayFrom,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  Promise,
  PromiseResolve,
  SafeMap,
  StringPrototypeSlice,
  StringPrototypePadStart,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;
const {
  Buffer: {
    concat: BufferConcat
  }
} = globalThis;
const {
  codes: {
    ERR_UNSUPPORTED_ESM_NETWORK_IMPORT_SPECIFIER,
    ERR_UNSUPPORTED_ESM_NETWORK_BAD_RESPONSE
  }
} = require('internal/errors');
const { URL } = require('internal/url');


/**
 * Lazily load these to avoid snapshot issues
 */

function HTTPSGet(url, opts) {
  // eslint-disable-next-line no-func-assign
  HTTPSGet = require('https').get;
  return HTTPSGet(url, opts);
}

function HTTPGet(url, opts) {
  // eslint-disable-next-line no-func-assign
  HTTPGet = require('http').get;
  return HTTPGet(url, opts);
}

function dnsLookup(name, opts) {
  // eslint-disable-next-line no-func-assign
  dnsLookup = require('dns/promises').lookup;
  return dnsLookup(name, opts);
}

/**
 * @param {URL} parsed
 * @returns {Promise<CacheEntry> | CacheEntry}
 */
function fetchWithRedirects(parsed) {
  const existing = getCache.get(parsed.href);
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
          const location = new URL(res.headers.location, parsed);
          if (location.protocol !== 'http:' && location.protocol !== 'https:') {
            reject(new ERR_UNSUPPORTED_ESM_NETWORK_IMPORT_SPECIFIER(
              res.headers.location,
              parsed.href,
              'cannot redirect to non-network location'));
            return;
          }
          return PromiseResolve(
            fetchWithRedirects(location)
          ).then((entry) => {
            getCache.set(parsed.href, entry);
            fulfill(entry);
          });

        }
      }
      if (res.statusCode > 303 || res.statusCode < 200) {
        dispose();
        reject(
          new ERR_UNSUPPORTED_ESM_NETWORK_BAD_RESPONSE(
            'HTTP response returned status code of ' + res.statusCode));
        return;
      }
      const { headers } = res;
      const contentType = headers['content-type'];
      if (!contentType) {
        dispose();
        reject(new ERR_UNSUPPORTED_ESM_NETWORK_BAD_RESPONSE(
          'content-type is required'));
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
          res.on('error', (e) => {
            dispose();
            reject(e);
            r(e);
          });
          const buffers = [];
          let size = 0;
          res.on('data', (d) => {
            ArrayPrototypePush(buffers, d);
            size += d.length;
          });
          res.on('end', () => {
            const body = entry.body = /** @type {Buffer} */(
              BufferConcat(buffers, size)
            );
            console.error('BODY', parsed.href, body+'')
            f(body);
          });
        }),
      };
      getCache.set(parsed.href, entry);
      fulfill(entry);
    });
  });
  getCache.set(parsed.href, result);
  return result;
}

/**
 * Returns if an address has local status by if it is going to a local
 * interface or is an address resolved by DNS to be a local interface
 * @param {string} hostname url.hostname to test
 * @returns {Promise<boolean>}
 */
async function isLocalAddress(hostname) {
  try {
    if (StringPrototypeStartsWith(hostname, '[') &&
        StringPrototypeEndsWith(hostname, ']') {
      hostname = StringPrototypeSlice(hostname, 1, -1);
    }
    const addr = await dnsLookup(hostname, { verbatim: true });
    if (addr.family === 4) {
      return addr.address === '127.0.0.1';
    } else if (addr.family === 6) {
      return normalizeIPv6(addr.address) ===
        '0000:0000:0000:0000:0000:0000:0000:0001';
    }
  } catch {}
  return false;
}

/**
 * Used to expand interface addresses to a canonical form for comparisons
 * E.G. comparing ::1, 0::1, etc. to 0000:0000:0000:0000:0000:0000:0000:0001
 * Due to IPv6 having multiple ways to represent a single address we need to
 * normalize
 * @param {string} str
 * @returns {string}
 */
function normalizeIPv6(str) {
  const parts = StringPrototypeSplit(str, ':');
  let expansionIndex = -1;
  for (let i = 0; i < parts.length; i++) {
    const part = parts[i];
    if (part === '' && expansionIndex === -1 && parts.length < 8) {
      expansionIndex = i;
      continue;
    }
    parts[i] = StringPrototypePadStart(part, 4, '0');
  }
  if (parts.length < 8) {
    ArrayPrototypeSplice(
      parts,
      expansionIndex,
      1,
      ...ArrayFrom({ length: 8 - parts.length + 1 }, () => '0000')
    );
  }
  return ArrayPrototypeJoin(parts, ':');
}

/**
 * @typedef CacheEntry
 * @property resolvedHREF {Promise<string> | string}
 * @property headers {Record<string, string>}
 * @property body {Promise<Buffer> | Buffer}
 */

/**
 * Only for GET requests, other requests would need new Map
 * HTTP cache semantics keep diff caches
 *
 * Maps HREF to pending cache entry
 * @type {Map<string, Promise<CacheEntry> | CacheEntry>}
 */
let getCache = new SafeMap();

/**
 * Fetches a location with a shared cache following redirects.
 * Does not respect HTTP cache headers.
 *
 * This splits the header and body Promises so that things only needing
 * headers don't need to wait on the body.
 * @param {URL} parsed
 * @returns {ReturnType<typeof fetchWithRedirects>}
 */
function fetch(parsed) {
  const { href } = parsed;
  const existing = getCache.get(href);
  if (existing) {
    return existing;
  }
  if (parsed.protocol === 'http:') {
    return isLocalAddress(parsed.hostname).then((is) => {
      if (is !== true) {
        throw new ERR_UNSUPPORTED_ESM_NETWORK_IMPORT_SPECIFIER(`Cannot load non-local address ${href} from network using http: protocol, use https: instead`);
      }
      return fetchWithRedirects(parsed);
    });
  }
  return fetchWithRedirects(parsed);
}

module.exports = {
  fetch
};
