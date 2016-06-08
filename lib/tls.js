'use strict';

const internalUtil = require('internal/util');
internalUtil.assertCrypto(exports);

const net = require('net');
const url = require('url');
const binding = process.binding('crypto');
const Buffer = require('buffer').Buffer;

// Allow {CLIENT_RENEG_LIMIT} client-initiated session renegotiations
// every {CLIENT_RENEG_WINDOW} seconds. An error event is emitted if more
// renegotations are seen. The settings are applied to all remote client
// connections.
exports.CLIENT_RENEG_LIMIT = 3;
exports.CLIENT_RENEG_WINDOW = 600;

exports.SLAB_BUFFER_SIZE = 10 * 1024 * 1024;

exports.DEFAULT_CIPHERS =
    process.binding('constants').crypto.defaultCipherList;

exports.DEFAULT_ECDH_CURVE = 'prime256v1';

exports.getCiphers = internalUtil.cachedResult(() => {
  return internalUtil.filterDuplicateStrings(binding.getSSLCiphers(), true);
});

// Convert protocols array into valid OpenSSL protocols list
// ("\x06spdy/2\x08http/1.1\x08http/1.0")
function convertProtocols(protocols) {
  const lens = Array(protocols.length);
  const buff = Buffer.allocUnsafe(protocols.reduce((p, c, i) => {
    var len = Buffer.byteLength(c);
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

exports.convertNPNProtocols = function(protocols, out) {
  // If protocols is Array - translate it into buffer
  if (Array.isArray(protocols)) {
    protocols = convertProtocols(protocols);
  }
  // If it's already a Buffer - store it
  if (protocols instanceof Buffer) {
    out.NPNProtocols = protocols;
  }
};

exports.convertALPNProtocols = function(protocols, out) {
  // If protocols is Array - translate it into buffer
  if (Array.isArray(protocols)) {
    protocols = convertProtocols(protocols);
  }
  // If it's already a Buffer - store it
  if (protocols instanceof Buffer) {
    // copy new buffer not to be modified by user
    out.ALPNProtocols = Buffer.from(protocols);
  }
};

exports.checkServerIdentity = function checkServerIdentity(host, cert) {
  // Create regexp to much hostnames
  function regexpify(host, wildcards) {
    // Add trailing dot (make hostnames uniform)
    if (!host || !host.endsWith('.')) host += '.';

    // The same applies to hostname with more than one wildcard,
    // if hostname has wildcard when wildcards are not allowed,
    // or if there are less than two dots after wildcard (i.e. *.com or *d.com)
    //
    // also
    //
    // "The client SHOULD NOT attempt to match a presented identifier in
    // which the wildcard character comprises a label other than the
    // left-most label (e.g., do not match bar.*.example.net)."
    // RFC6125
    if (!wildcards && /\*/.test(host) || /[\.\*].*\*/.test(host) ||
        /\*/.test(host) && !/\*.*\..+\..+/.test(host)) {
      return /$./;
    }

    // Replace wildcard chars with regexp's wildcard and
    // escape all characters that have special meaning in regexps
    // (i.e. '.', '[', '{', '*', and others)
    var re = host.replace(
        /\*([a-z0-9\\-_\.])|[\.,\-\\\^\$+?*\[\]\(\):!\|{}]/g,
        function(all, sub) {
          if (sub) return '[a-z0-9\\-_]*' + (sub === '-' ? '\\-' : sub);
          return '\\' + all;
        });

    return new RegExp('^' + re + '$', 'i');
  }

  var dnsNames = [];
  var uriNames = [];
  const ips = [];
  var matchCN = true;
  var valid = false;
  var reason = 'Unknown reason';

  // There're several names to perform check against:
  // CN and altnames in certificate extension
  // (DNS names, IP addresses, and URIs)
  //
  // Walk through altnames and generate lists of those names
  if (cert.subjectaltname) {
    cert.subjectaltname.split(/, /g).forEach(function(altname) {
      var option = altname.match(/^(DNS|IP Address|URI):(.*)$/);
      if (!option)
        return;
      if (option[1] === 'DNS') {
        dnsNames.push(option[2]);
      } else if (option[1] === 'IP Address') {
        ips.push(option[2]);
      } else if (option[1] === 'URI') {
        var uri = url.parse(option[2]);
        if (uri) uriNames.push(uri.hostname);
      }
    });
  }

  // If hostname is an IP address, it should be present in the list of IP
  // addresses.
  if (net.isIP(host)) {
    valid = ips.some(function(ip) {
      return ip === host;
    });
    if (!valid) {
      reason = `IP: ${host} is not in the cert's list: ${ips.join(', ')}`;
    }
  } else if (cert.subject) {
    // Transform hostname to canonical form
    if (!host || !host.endsWith('.')) host += '.';

    // Otherwise check all DNS/URI records from certificate
    // (with allowed wildcards)
    dnsNames = dnsNames.map(function(name) {
      return regexpify(name, true);
    });

    // Wildcards ain't allowed in URI names
    uriNames = uriNames.map(function(name) {
      return regexpify(name, false);
    });

    dnsNames = dnsNames.concat(uriNames);

    if (dnsNames.length > 0) matchCN = false;

    // Match against Common Name (CN) only if no supported identifiers are
    // present.
    //
    // "As noted, a client MUST NOT seek a match for a reference identifier
    //  of CN-ID if the presented identifiers include a DNS-ID, SRV-ID,
    //  URI-ID, or any application-specific identifier types supported by the
    //  client."
    // RFC6125
    if (matchCN) {
      var commonNames = cert.subject.CN;
      if (Array.isArray(commonNames)) {
        for (var i = 0, k = commonNames.length; i < k; ++i) {
          dnsNames.push(regexpify(commonNames[i], true));
        }
      } else {
        dnsNames.push(regexpify(commonNames, true));
      }
    }

    valid = dnsNames.some(function(re) {
      return re.test(host);
    });

    if (!valid) {
      if (cert.subjectaltname) {
        reason =
            `Host: ${host} is not in the cert's altnames: ` +
            `${cert.subjectaltname}`;
      } else {
        reason = `Host: ${host} is not cert's CN: ${cert.subject.CN}`;
      }
    }
  } else {
    reason = 'Cert is empty';
  }

  if (!valid) {
    var err = new Error(
        `Hostname/IP doesn't match certificate's altnames: "${reason}"`);
    err.reason = reason;
    err.host = host;
    err.cert = cert;
    return err;
  }
};

// Example:
// C=US\nST=CA\nL=SF\nO=Joyent\nOU=Node.js\nCN=ca1\nemailAddress=ry@clouds.org
exports.parseCertString = function parseCertString(s) {
  var out = {};
  var parts = s.split('\n');
  for (var i = 0, len = parts.length; i < len; i++) {
    var sepIndex = parts[i].indexOf('=');
    if (sepIndex > 0) {
      var key = parts[i].slice(0, sepIndex);
      var value = parts[i].slice(sepIndex + 1);
      if (key in out) {
        if (!Array.isArray(out[key])) {
          out[key] = [out[key]];
        }
        out[key].push(value);
      } else {
        out[key] = value;
      }
    }
  }
  return out;
};

// Public API
exports.createSecureContext = require('_tls_common').createSecureContext;
exports.SecureContext = require('_tls_common').SecureContext;
exports.TLSSocket = require('_tls_wrap').TLSSocket;
exports.Server = require('_tls_wrap').Server;
exports.createServer = require('_tls_wrap').createServer;
exports.connect = require('_tls_wrap').connect;
exports.createSecurePair = require('_tls_legacy').createSecurePair;
