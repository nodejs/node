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
  Array,
  ArrayIsArray,
  // eslint-disable-next-line no-restricted-syntax
  ArrayPrototypePush,
  JSONParse,
  ObjectDefineProperty,
  ObjectFreeze,
  StringFromCharCode,
} = primordials;

const {
  ERR_TLS_CERT_ALTNAME_FORMAT,
  ERR_TLS_CERT_ALTNAME_INVALID,
  ERR_OUT_OF_RANGE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_ARG_TYPE,
} = require('internal/errors').codes;
const internalUtil = require('internal/util');
internalUtil.assertCrypto();
const {
  isArrayBufferView,
  isUint8Array,
} = require('internal/util/types');

const net = require('net');
const { getOptionValue } = require('internal/options');
const {
  getBundledRootCertificates,
  getExtraCACertificates,
  getSystemCACertificates,
  resetRootCertStore,
  getUserRootCertificates,
  getSSLCiphers,
} = internalBinding('crypto');
const { Buffer } = require('buffer');
const { canonicalizeIP } = internalBinding('cares_wrap');
const tlsCommon = require('internal/tls/common');
const tlsWrap = require('internal/tls/wrap');
const {
  validateOneOf,
  validateString,
} = require('internal/validators');
const { X509Certificate } = require('crypto');

// Allow {CLIENT_RENEG_LIMIT} client-initiated session renegotiations
// every {CLIENT_RENEG_WINDOW} seconds. An error event is emitted if more
// renegotiations are seen. The settings are applied to all remote client
// connections.
exports.CLIENT_RENEG_LIMIT = 3;
exports.CLIENT_RENEG_WINDOW = 600;

exports.DEFAULT_CIPHERS = getOptionValue('--tls-cipher-list');

exports.DEFAULT_ECDH_CURVE = 'auto';

if (getOptionValue('--tls-min-v1.0'))
  exports.DEFAULT_MIN_VERSION = 'TLSv1';
else if (getOptionValue('--tls-min-v1.1'))
  exports.DEFAULT_MIN_VERSION = 'TLSv1.1';
else if (getOptionValue('--tls-min-v1.2'))
  exports.DEFAULT_MIN_VERSION = 'TLSv1.2';
else if (getOptionValue('--tls-min-v1.3'))
  exports.DEFAULT_MIN_VERSION = 'TLSv1.3';
else
  exports.DEFAULT_MIN_VERSION = 'TLSv1.2';

if (getOptionValue('--tls-max-v1.3'))
  exports.DEFAULT_MAX_VERSION = 'TLSv1.3';
else if (getOptionValue('--tls-max-v1.2'))
  exports.DEFAULT_MAX_VERSION = 'TLSv1.2';
else
  exports.DEFAULT_MAX_VERSION = 'TLSv1.3'; // Will depend on node version.


exports.getCiphers = internalUtil.cachedResult(
  () => internalUtil.filterDuplicateStrings(getSSLCiphers(), true),
);

let bundledRootCertificates;
function cacheBundledRootCertificates() {
  bundledRootCertificates ||= ObjectFreeze(getBundledRootCertificates());

  return bundledRootCertificates;
}

ObjectDefineProperty(exports, 'rootCertificates', {
  __proto__: null,
  configurable: false,
  enumerable: true,
  get: cacheBundledRootCertificates,
});

let extraCACertificates;
function cacheExtraCACertificates() {
  extraCACertificates ||= ObjectFreeze(getExtraCACertificates());

  return extraCACertificates;
}

let systemCACertificates;
function cacheSystemCACertificates() {
  systemCACertificates ||= ObjectFreeze(getSystemCACertificates());

  return systemCACertificates;
}

let defaultCACertificates;
let hasResetDefaultCACertificates = false;

function cacheDefaultCACertificates() {
  if (defaultCACertificates) { return defaultCACertificates; }

  if (hasResetDefaultCACertificates) {
    defaultCACertificates = getUserRootCertificates();
    ObjectFreeze(defaultCACertificates);
    return defaultCACertificates;
  }

  defaultCACertificates = [];

  if (!getOptionValue('--use-openssl-ca')) {
    const bundled = cacheBundledRootCertificates();
    for (let i = 0; i < bundled.length; ++i) {
      ArrayPrototypePush(defaultCACertificates, bundled[i]);
    }
    if (getOptionValue('--use-system-ca')) {
      const system = cacheSystemCACertificates();
      for (let i = 0; i < system.length; ++i) {

        ArrayPrototypePush(defaultCACertificates, system[i]);
      }
    }
  }

  if (process.env.NODE_EXTRA_CA_CERTS) {
    const extra = cacheExtraCACertificates();
    for (let i = 0; i < extra.length; ++i) {

      ArrayPrototypePush(defaultCACertificates, extra[i]);
    }
  }

  ObjectFreeze(defaultCACertificates);
  return defaultCACertificates;
}

function getCACertificates(options = {}) {
  if (typeof options === 'string') {
    options = { type: options };
  } else if (typeof options !== 'object' || options === null) {
    throw new ERR_INVALID_ARG_TYPE('options', ['string', 'object'], options);
  }

  const {
    type = 'default',
    format = 'string',
  } = options;

  validateString(type, 'type');
  validateOneOf(format, 'format', ['string', 'buffer', 'x509']);

  let certs;
  switch (type) {
    case 'default': certs = cacheDefaultCACertificates(); break;
    case 'bundled': certs = cacheBundledRootCertificates(); break;
    case 'system': certs = cacheSystemCACertificates(); break;
    case 'extra': certs = cacheExtraCACertificates(); break;
    default: throw new ERR_INVALID_ARG_VALUE('type', type);
  }

  if (format === 'string') {
    // Return PEM strings directly
    return certs.map((cert) => {
      if (typeof cert === 'string') return cert;
      if (Buffer.isBuffer(cert)) return cert.toString('ascii');
      throw new ERR_INVALID_ARG_VALUE('cert', cert);
    });
  }

  const buffers = certs.map((cert) => {
    if (Buffer.isBuffer(cert)) return cert;
    if (typeof cert === 'string') {
      const base64 = cert
        .replace(/-----BEGIN CERTIFICATE-----/g, '')
        .replace(/-----END CERTIFICATE-----/g, '')
        .replace(/\s+/g, '');
      return Buffer.from(base64, 'base64');
    }
    throw new ERR_INVALID_ARG_VALUE('cert', cert);
  });

  if (format === 'buffer') {
    return buffers;
  }

  return buffers.map((buf) => new X509Certificate(buf));
}

exports.getCACertificates = getCACertificates;

function setDefaultCACertificates(certs) {
  if (!ArrayIsArray(certs)) {
    throw new ERR_INVALID_ARG_TYPE('certs', 'Array', certs);
  }

  // Verify that all elements in the array are strings
  for (let i = 0; i < certs.length; i++) {
    if (typeof certs[i] !== 'string' && !isArrayBufferView(certs[i])) {
      throw new ERR_INVALID_ARG_TYPE(
        `certs[${i}]`, ['string', 'ArrayBufferView'], certs[i]);
    }
  }

  resetRootCertStore(certs);
  defaultCACertificates = undefined; // Reset the cached default certificates
  hasResetDefaultCACertificates = true;
}

exports.setDefaultCACertificates = setDefaultCACertificates;

// Convert protocols array into valid OpenSSL protocols list
// ("\x06spdy/2\x08http/1.1\x08http/1.0")
function convertProtocols(protocols) {
  const lens = new Array(protocols.length);
  const buff = Buffer.allocUnsafe(protocols.reduce((p, c, i) => {
    const len = Buffer.byteLength(c);
    if (len > 255) {
      throw new ERR_OUT_OF_RANGE('The byte length of the protocol at index ' +
        `${i} exceeds the maximum length.`, '<= 255', len, true);
    }
    lens[i] = len;
    return p + 1 + len;
  }, 0));

  let offset = 0;
  for (let i = 0, c = protocols.length; i < c; i++) {
    buff[offset++] = lens[i];
    buff.write(protocols[i], offset);
    offset += lens[i];
  }

  return buff;
}

exports.convertALPNProtocols = function convertALPNProtocols(protocols, out) {
  // If protocols is Array - translate it into buffer
  if (ArrayIsArray(protocols)) {
    out.ALPNProtocols = convertProtocols(protocols);
  } else if (isUint8Array(protocols)) {
    // Copy new buffer not to be modified by user.
    out.ALPNProtocols = Buffer.from(protocols);
  } else if (isArrayBufferView(protocols)) {
    out.ALPNProtocols = Buffer.from(protocols.buffer.slice(
      protocols.byteOffset,
      protocols.byteOffset + protocols.byteLength,
    ));
  }
};

function unfqdn(host) {
  return host.replace(/[.]$/, '');
}

// String#toLowerCase() is locale-sensitive so we use
// a conservative version that only lowercases A-Z.
function toLowerCase(c) {
  return StringFromCharCode(32 + c.charCodeAt(0));
}

function splitHost(host) {
  return unfqdn(host).replace(/[A-Z]/g, toLowerCase).split('.');
}

function check(hostParts, pattern, wildcards) {
  // Empty strings, null, undefined, etc. never match.
  if (!pattern)
    return false;

  const patternParts = splitHost(pattern);

  if (hostParts.length !== patternParts.length)
    return false;

  // Pattern has empty components, e.g. "bad..example.com".
  if (patternParts.includes(''))
    return false;

  // RFC 6125 allows IDNA U-labels (Unicode) in names but we have no
  // good way to detect their encoding or normalize them so we simply
  // reject them.  Control characters and blanks are rejected as well
  // because nothing good can come from accepting them.
  const isBad = (s) => /[^\u0021-\u007F]/u.test(s);
  if (patternParts.some(isBad))
    return false;

  // Check host parts from right to left first.
  for (let i = hostParts.length - 1; i > 0; i -= 1) {
    if (hostParts[i] !== patternParts[i])
      return false;
  }

  const hostSubdomain = hostParts[0];
  const patternSubdomain = patternParts[0];
  const patternSubdomainParts = patternSubdomain.split('*', 3);

  // Short-circuit when the subdomain does not contain a wildcard.
  // RFC 6125 does not allow wildcard substitution for components
  // containing IDNA A-labels (Punycode) so match those verbatim.
  if (patternSubdomainParts.length === 1 ||
      patternSubdomain.includes('xn--'))
    return hostSubdomain === patternSubdomain;

  if (!wildcards)
    return false;

  // More than one wildcard is always wrong.
  if (patternSubdomainParts.length > 2)
    return false;

  // *.tld wildcards are not allowed.
  if (patternParts.length <= 2)
    return false;

  const { 0: prefix, 1: suffix } = patternSubdomainParts;

  if (prefix.length + suffix.length > hostSubdomain.length)
    return false;

  if (!hostSubdomain.startsWith(prefix))
    return false;

  if (!hostSubdomain.endsWith(suffix))
    return false;

  return true;
}

// This pattern is used to determine the length of escaped sequences within
// the subject alt names string. It allows any valid JSON string literal.
// This MUST match the JSON specification (ECMA-404 / RFC8259) exactly.
const jsonStringPattern =
  // eslint-disable-next-line no-control-regex
  /^"(?:[^"\\\u0000-\u001f]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"/;

function splitEscapedAltNames(altNames) {
  const result = [];
  let currentToken = '';
  let offset = 0;
  while (offset !== altNames.length) {
    const nextSep = altNames.indexOf(',', offset);
    const nextQuote = altNames.indexOf('"', offset);
    if (nextQuote !== -1 && (nextSep === -1 || nextQuote < nextSep)) {
      // There is a quote character and there is no separator before the quote.
      currentToken += altNames.substring(offset, nextQuote);
      const match = jsonStringPattern.exec(altNames.substring(nextQuote));
      if (!match) {
        throw new ERR_TLS_CERT_ALTNAME_FORMAT();
      }
      currentToken += JSONParse(match[0]);
      offset = nextQuote + match[0].length;
    } else if (nextSep !== -1) {
      // There is a separator and no quote before it.
      currentToken += altNames.substring(offset, nextSep);
      result.push(currentToken);
      currentToken = '';
      offset = nextSep + 2;
    } else {
      currentToken += altNames.substring(offset);
      offset = altNames.length;
    }
  }
  result.push(currentToken);
  return result;
}

exports.checkServerIdentity = function checkServerIdentity(hostname, cert) {
  const subject = cert.subject;
  const altNames = cert.subjectaltname;
  const dnsNames = [];
  const ips = [];

  hostname = '' + hostname;

  if (altNames) {
    const splitAltNames = altNames.includes('"') ?
      splitEscapedAltNames(altNames) :
      altNames.split(', ');
    splitAltNames.forEach((name) => {
      if (name.startsWith('DNS:')) {
        dnsNames.push(name.slice(4));
      } else if (name.startsWith('IP Address:')) {
        ips.push(canonicalizeIP(name.slice(11)));
      }
    });
  }

  let valid = false;
  let reason = 'Unknown reason';

  hostname = unfqdn(hostname);  // Remove trailing dot for error messages.

  if (net.isIP(hostname)) {
    valid = ips.includes(canonicalizeIP(hostname));
    if (!valid)
      reason = `IP: ${hostname} is not in the cert's list: ` + ips.join(', ');
  } else if (dnsNames.length > 0 || subject?.CN) {
    const hostParts = splitHost(hostname);
    const wildcard = (pattern) => check(hostParts, pattern, true);

    if (dnsNames.length > 0) {
      valid = dnsNames.some(wildcard);
      if (!valid)
        reason =
          `Host: ${hostname}. is not in the cert's altnames: ${altNames}`;
    } else {
      // Match against Common Name only if no supported identifiers exist.
      const cn = subject.CN;

      if (ArrayIsArray(cn))
        valid = cn.some(wildcard);
      else if (cn)
        valid = wildcard(cn);

      if (!valid)
        reason = `Host: ${hostname}. is not cert's CN: ${cn}`;
    }
  } else {
    reason = 'Cert does not contain a DNS name';
  }

  if (!valid) {
    return new ERR_TLS_CERT_ALTNAME_INVALID(reason, hostname, cert);
  }
};

exports.createSecureContext = tlsCommon.createSecureContext;
exports.SecureContext = tlsCommon.SecureContext;
exports.TLSSocket = tlsWrap.TLSSocket;
exports.Server = tlsWrap.Server;
exports.createServer = tlsWrap.createServer;
exports.connect = tlsWrap.connect;
