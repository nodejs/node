'use strict';

const {
  certExportChallenge,
  certExportPublicKey,
  certVerifySpkac,
} = internalBinding('crypto');

const {
  getArrayBufferOrView,
} = require('internal/crypto/util');

// The functions contained in this file cover the SPKAC format
// (also referred to as Netscape SPKI). A general description of
// the format can be found at https://en.wikipedia.org/wiki/SPKAC

function verifySpkac(spkac, encoding) {
  return certVerifySpkac(
    getArrayBufferOrView(spkac, 'spkac', encoding));
}

function exportPublicKey(spkac, encoding) {
  return certExportPublicKey(
    getArrayBufferOrView(spkac, 'spkac', encoding));
}

function exportChallenge(spkac, encoding) {
  return certExportChallenge(
    getArrayBufferOrView(spkac, 'spkac', encoding));
}

// The legacy implementation of this exposed the Certificate
// object and required that users create an instance before
// calling the member methods. This API pattern has been
// deprecated, however, as the method implementations do not
// rely on any object state.

// For backwards compatibility reasons, this cannot be converted into a
// ES6 Class.
function Certificate() {
  if (!(this instanceof Certificate))
    return new Certificate();
}

Certificate.prototype.verifySpkac = verifySpkac;
Certificate.prototype.exportPublicKey = exportPublicKey;
Certificate.prototype.exportChallenge = exportChallenge;

Certificate.exportChallenge = exportChallenge;
Certificate.exportPublicKey = exportPublicKey;
Certificate.verifySpkac = verifySpkac;

module.exports = Certificate;
