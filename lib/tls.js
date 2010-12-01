var crypto = require('crypto');
var net = require('net');
var events = require('events');
var inherits = require('util').inherits;

// TODO: support anonymous (nocert) and PSK
// TODO: how to proxy maxConnections?


// Options:
// - unauthorizedPeers. Boolean, default to false.
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
function Server ( /* [options], listener */) {
  var options, listener;
  if (typeof arguments[0] == "object") {
    options = arguments[0];
    listener = arguments[1];
  } else if (typeof arguments[0] == "function") {
    options = {};
    listener = arguments[0];
  }

  if (!(this instanceof Server)) return new Server(options, listener);

  var self = this;

  // constructor call
  net.Server.call(this, function (socket) {
    var creds = crypto.createCredentials({ key: self.key,
                                           cert: self.cert,
                                           ca: self.ca });
    creds.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

    var pair = crypto.createPair(creds,
                                 true,
                                 !self.unauthorizedPeers);
    pair.encrypted.pipe(socket);
    socket.pipe(pair.encrypted);

    pair.on('secure', function () {
      var verifyError = pair._ssl.verifyError();

      if (verifyError) {
        if (self.unauthorizedPeers) {
          self.emit('unauthorized', pair.cleartext, verifyError);
        } else {
          console.error("REJECT PEER. verify error: %s", verifyError);
          socket.destroy();
        }
      } else {
        self.emit('authorized', pair.cleartext);
      }
    });

    pair.on('error', function (e) {
      console.log('pair got error: ' + e);

      // TODO better way to get error code.
      if (/no shared cipher/.test(e.message)) {
        ;
      } else {
        self.emit('error', e);
      }
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
exports.createServer = function (options, listener) {
  return new Server(options, listener);
};


Server.prototype.setOptions = function (options) {
  if (typeof options.unauthorizedPeers == "boolean") {
    this.unauthorizedPeers = options.unauthorizedPeers;
  } else {
    this.unauthorizedPeers = false;
  }

  if (options.key) this.key = options.key;
  if (options.cert) this.cert = options.cert;
  if (options.ca) this.ca = options.ca;
};

