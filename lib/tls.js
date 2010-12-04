var crypto = require('crypto');
var securepair = require('securepair');
var net = require('net');
var events = require('events');
var inherits = require('util').inherits;

var assert = process.assert;

// TODO: support anonymous (nocert) and PSK
// TODO: how to proxy maxConnections?


// AUTHENTICATION MODES
//
// There are several levels of authentication that TLS/SSL supports.
// Read more about this in "man SSL_set_verify".
//
// 1. The server sends a certificate to the client but does not request a
// cert from the client. This is common for most HTTPS servers. The browser
// can verify the identity of the server, but the server does not know who
// the client is. Authenticating the client is usually done over HTTP using
// login boxes and cookies and stuff.
//
// 2. The server sends a cert to the client and requests that the client
// also send it a cert. The client knows who the server is and the server is
// requesting the client also identify themselves. There are several
// outcomes:
//
//   A) verifyError returns null meaning the client's certificate is signed
//   by one of the server's CAs. The server know's the client idenity now
//   and the client is authorized.
//
//   B) For some reason the client's certificate is not acceptable -
//   verifyError returns a string indicating the problem. The server can
//   either (i) reject the client or (ii) allow the client to connect as an
//   unauthorized connection.
//
// The mode is controlled by two boolean variables.
//
// requestCert
//   If true the server requests a certificate from client connections. For
//   the common HTTPS case, users will want this to be false, which is what
//   it defaults to.
//
// rejectUnauthorized
//   If true clients whose certificates are invalid for any reason will not
//   be allowed to make connections. If false, they will simply be marked as
//   unauthorized but secure communication will continue. By default this is
//   false.
//
//
//
// Options:
// - requestCert. Send verify request. Default to false.
// - rejectUnauthorized. Boolean, default to false.
// - key. string.
// - cert: string.
// - ca: string or array of strings.
//
// emit 'authorized'
//   function (cleartext) { }
//
// emit 'unauthorized'
//   function (cleartext, verifyError) { }
//   Possible errors:
//   "UNABLE_TO_GET_ISSUER_CERT", "UNABLE_TO_GET_CRL",
//   "UNABLE_TO_DECRYPT_CERT_SIGNATURE", "UNABLE_TO_DECRYPT_CRL_SIGNATURE",
//   "UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY", "CERT_SIGNATURE_FAILURE",
//   "CRL_SIGNATURE_FAILURE", "CERT_NOT_YET_VALID" "CERT_HAS_EXPIRED",
//   "CRL_NOT_YET_VALID", "CRL_HAS_EXPIRED" "ERROR_IN_CERT_NOT_BEFORE_FIELD",
//   "ERROR_IN_CERT_NOT_AFTER_FIELD", "ERROR_IN_CRL_LAST_UPDATE_FIELD",
//   "ERROR_IN_CRL_NEXT_UPDATE_FIELD", "OUT_OF_MEM",
//   "DEPTH_ZERO_SELF_SIGNED_CERT", "SELF_SIGNED_CERT_IN_CHAIN",
//   "UNABLE_TO_GET_ISSUER_CERT_LOCALLY", "UNABLE_TO_VERIFY_LEAF_SIGNATURE",
//   "CERT_CHAIN_TOO_LONG", "CERT_REVOKED" "INVALID_CA",
//   "PATH_LENGTH_EXCEEDED", "INVALID_PURPOSE" "CERT_UNTRUSTED",
//   "CERT_REJECTED"
//
//
// TODO:
// cleartext.credentials (by mirroring from pair object)
// cleartext.getCertificate() (by mirroring from pair.credentials.context)
function Server(/* [options], listener */) {
  var options, listener;
  if (typeof arguments[0] == 'object') {
    options = arguments[0];
    listener = arguments[1];
  } else if (typeof arguments[0] == 'function') {
    options = {};
    listener = arguments[0];
  }

  if (!(this instanceof Server)) return new Server(options, listener);

  var self = this;

  // constructor call
  net.Server.call(this, function(socket) {
    var creds = crypto.createCredentials(
        { key: self.key, cert: self.cert, ca: self.ca });
    creds.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

    var pair = securepair.createSecurePair(creds,
                                           true,
                                           self.requestCert,
                                           self.rejectUnauthorized);
    pair.encrypted.pipe(socket);
    socket.pipe(pair.encrypted);

    pair.on('secure', function(verifyError) {
      if (!self.requestCert) {
        self.emit('unauthorized', pair.cleartext);
      } else {
        var verifyError = pair._ssl.verifyError();
        if (verifyError) {
          self.emit('unauthorized', pair.cleartext, verifyError);
        } else {
          self.emit('authorized', pair.cleartext);
        }
      }
    });

    pair.on('error', function(e) {
      console.log('pair got error: ' + e);
      self.emit('error', e);
    });

    pair.cleartext.on('error', function(err) {
      console.log('cleartext got error: ' + err);
    });

    pair.encrypted.on('error', function(err) {
      console.log('encrypted got error: ' + err);
    });
  });

  if (listener) {
    this.on('authorized', listener);
    this.on('unauthorized', listener);
  }

  // Handle option defaults:
  this.setOptions(options);
}

inherits(Server, net.Server);
exports.Server = Server;
exports.createServer = function(options, listener) {
  return new Server(options, listener);
};


Server.prototype.setOptions = function(options) {
  if (typeof options.requestCert == 'boolean') {
    this.requestCert = options.requestCert;
  } else {
    this.requestCert = false;
  }

  if (typeof options.rejectUnauthorized == 'boolean') {
    this.rejectUnauthorized = options.rejectUnauthorized;
  } else {
    this.rejectUnauthorized = false;
  }

  if (options.key) this.key = options.key;
  if (options.cert) this.cert = options.cert;
  if (options.ca) this.ca = options.ca;
};

