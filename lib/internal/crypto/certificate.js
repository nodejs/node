'use strict';

const {
  certExportChallenge,
  certExportPublicKey,
  certVerifySpkac
} = internalBinding('crypto');
const {
  validateBuffer
} = require('internal/validators');

const {
  getArrayBufferView
} = require('internal/crypto/util');

function verifySpkac(spkac) {
  validateBuffer(spkac, 'spkac');
  return certVerifySpkac(spkac);
}

function exportPublicKey(spkac, encoding) {
  return certExportPublicKey(
    getArrayBufferView(spkac, 'spkac', encoding)
  );
}

function exportChallenge(spkac, encoding) {
  return certExportChallenge(
    getArrayBufferView(spkac, 'spkac', encoding)
  );
}

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
