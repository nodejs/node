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
  ERR_TLS_CERT_ALTNAME_INVALID,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const internalUtil = require('internal/util');
const internalTLS = require('internal/tls');
internalUtil.assertCrypto();
const { isArrayBufferView } = require('internal/util/types');

const net = require('net');
const { getOptionValue } = require('internal/options');
const url = require('url');
const binding = internalBinding('crypto');
const { Buffer } = require('buffer');
const EventEmitter = require('events');
const { URL } = require('internal/url');
const DuplexPair = require('internal/streams/duplexpair');
const { canonicalizeIP } = internalBinding('cares_wrap');
const _tls_common = require('_tls_common');
const _tls_wrap = require('_tls_wrap');

// Allow {CLIENT_RENEG_LIMIT} client-initiated session renegotiations
// every {CLIENT_RENEG_WINDOW} seconds. An error event is emitted if more
// renegotiations are seen. The settings are applied to all remote client
// connections.
exports.CLIENT_RENEG_LIMIT = 3;
exports.CLIENT_RENEG_WINDOW = 600;

exports.DEFAULT_CIPHERS =
    internalBinding('constants').crypto.defaultCipherList;

exports.DEFAULT_ECDH_CURVE = 'auto';

exports.DEFAULT_MAX_VERSION = 'TLSv1.2';

if (getOptionValue('--tls-v1.0'))
  exports.DEFAULT_MIN_VERSION = 'TLSv1';
else if (getOptionValue('--tls-v1.1'))
  exports.DEFAULT_MIN_VERSION = 'TLSv1.1';
else
  exports.DEFAULT_MIN_VERSION = 'TLSv1.2';

exports.getCiphers = internalUtil.cachedResult(
  () => internalUtil.filterDuplicateStrings(binding.getSSLCiphers(), true)
);

// Convert protocols array into valid OpenSSL protocols list
// ("\x06spdy/2\x08http/1.1\x08http/1.0")
function convertProtocols(protocols) {
  const lens = new Array(protocols.length);
  const buff = Buffer.allocUnsafe(protocols.reduce((p, c, i) => {
    var len = Buffer.byteLength(c);
    if (len > 255) {
      throw new ERR_OUT_OF_RANGE('The byte length of the protocol at index ' +
        `${i} exceeds the maximum length.`, '<= 255', len, true);
    }
    lens[i] = len;
    return p + 1 + len;
  }, 0));

  var offset = 0;
  for (var i = 0, c = protocols.length; i < c; i++) {
    buff[offset++] = lens[i];
    buff.write(protocols[i], offset);
    offset += lens[i];
  }

  return buff;
}

exports.convertALPNProtocols = function convertALPNProtocols(protocols, out) {
  // If protocols is Array - translate it into buffer
  if (Array.isArray(protocols)) {
    out.ALPNProtocols = convertProtocols(protocols);
  } else if (isArrayBufferView(protocols)) {
    // Copy new buffer not to be modified by user.
    out.ALPNProtocols = Buffer.from(protocols);
  }
};

function unfqdn(host) {
  return host.replace(/[.]$/, '');
}

function splitHost(host) {
  // String#toLowerCase() is locale-sensitive so we use
  // a conservative version that only lowercases A-Z.
  const replacer = (c) => String.fromCharCode(32 + c.charCodeAt(0));
  return unfqdn(host).replace(/[A-Z]/g, replacer).split('.');
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
  for (var i = hostParts.length - 1; i > 0; i -= 1) {
    if (hostParts[i] !== patternParts[i])
      return false;
  }

  const hostSubdomain = hostParts[0];
  const patternSubdomain = patternParts[0];
  const patternSubdomainParts = patternSubdomain.split('*');

  // Short-circuit when the subdomain does not contain a wildcard.
  // RFC 6125 does not allow wildcard substitution for components
  // containing IDNA A-labels (Punycode) so match those verbatim.
  if (patternSubdomainParts.length === 1 || patternSubdomain.includes('xn--'))
    return hostSubdomain === patternSubdomain;

  if (!wildcards)
    return false;

  // More than one wildcard is always wrong.
  if (patternSubdomainParts.length > 2)
    return false;

  // *.tld wildcards are not allowed.
  if (patternParts.length <= 2)
    return false;

  const [prefix, suffix] = patternSubdomainParts;

  if (prefix.length + suffix.length > hostSubdomain.length)
    return false;

  if (!hostSubdomain.startsWith(prefix))
    return false;

  if (!hostSubdomain.endsWith(suffix))
    return false;

  return true;
}

let urlWarningEmitted = false;
exports.checkServerIdentity = function checkServerIdentity(hostname, cert) {
  const subject = cert.subject;
  const altNames = cert.subjectaltname;
  const dnsNames = [];
  const uriNames = [];
  const ips = [];

  hostname = '' + hostname;

  if (altNames) {
    for (const name of altNames.split(', ')) {
      if (name.startsWith('DNS:')) {
        dnsNames.push(name.slice(4));
      } else if (name.startsWith('URI:')) {
        let uri;
        try {
          uri = new URL(name.slice(4));
        } catch {
          uri = url.parse(name.slice(4));
          if (!urlWarningEmitted && !process.noDeprecation) {
            urlWarningEmitted = true;
            process.emitWarning(
              `The URI ${name.slice(4)} found in cert.subjectaltname ` +
              'is not a valid URI, and is supported in the tls module ' +
              'solely for compatibility.',
              'DeprecationWarning', 'DEP0109');
          }
        }

        uriNames.push(uri.hostname);  // TODO(bnoordhuis) Also use scheme.
      } else if (name.startsWith('IP Address:')) {
        ips.push(canonicalizeIP(name.slice(11)));
      }
    }
  }

  let valid = false;
  let reason = 'Unknown reason';

  if (net.isIP(hostname)) {
    valid = ips.includes(canonicalizeIP(hostname));
    if (!valid)
      reason = `IP: ${hostname} is not in the cert's list: ${ips.join(', ')}`;
    // TODO(bnoordhuis) Also check URI SANs that are IP addresses.
  } else if (subject) {
    hostname = unfqdn(hostname);  // Remove trailing dot for error messages.
    const hostParts = splitHost(hostname);
    const wildcard = (pattern) => check(hostParts, pattern, true);
    const noWildcard = (pattern) => check(hostParts, pattern, false);

    // Match against Common Name only if no supported identifiers are present.
    if (dnsNames.length === 0 && ips.length === 0 && uriNames.length === 0) {
      const cn = subject.CN;

      if (Array.isArray(cn))
        valid = cn.some(wildcard);
      else if (cn)
        valid = wildcard(cn);

      if (!valid)
        reason = `Host: ${hostname}. is not cert's CN: ${cn}`;
    } else {
      valid = dnsNames.some(wildcard) || uriNames.some(noWildcard);
      if (!valid)
        reason =
          `Host: ${hostname}. is not in the cert's altnames: ${altNames}`;
    }
  } else {
    reason = 'Cert is empty';
  }

  if (!valid) {
    const err = new ERR_TLS_CERT_ALTNAME_INVALID(reason);
    err.reason = reason;
    err.host = hostname;
    err.cert = cert;
    return err;
  }
};


class SecurePair extends EventEmitter {
  constructor(secureContext = exports.createSecureContext(),
              isServer = false,
              requestCert = !isServer,
              rejectUnauthorized = false,
              options = {}) {
    super();
    const { socket1, socket2 } = new DuplexPair();

    this.server = options.server;
    this.credentials = secureContext;

    this.encrypted = socket1;
    this.cleartext = new exports.TLSSocket(socket2, {
      secureContext,
      isServer,
      requestCert,
      rejectUnauthorized,
      ...options
    });
    this.cleartext.once('secure', () => this.emit('secure'));
  }

  destroy() {
    this.cleartext.destroy();
    this.encrypted.destroy();
  }
}


exports.parseCertString = internalUtil.deprecate(
  internalTLS.parseCertString,
  'tls.parseCertString() is deprecated. ' +
  'Please use querystring.parse() instead.',
  'DEP0076');

exports.createSecureContext = _tls_common.createSecureContext;
exports.SecureContext = _tls_common.SecureContext;
exports.TLSSocket = _tls_wrap.TLSSocket;
exports.Server = _tls_wrap.Server;
exports.createServer = _tls_wrap.createServer;
exports.connect = _tls_wrap.connect;

exports.createSecurePair = internalUtil.deprecate(
  function createSecurePair(...args) {
    return new SecurePair(...args);
  },
  'tls.createSecurePair() is deprecated. Please use ' +
  'tls.TLSSocket instead.', 'DEP0064');
