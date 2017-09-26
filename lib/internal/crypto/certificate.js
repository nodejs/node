'use strict';

const {
  certExportChallenge,
  certExportPublicKey,
  certVerifySpkac
} = process.binding('crypto');

const {
  toBuf
} = require('internal/crypto/util');

function verifySpkac(object) {
  return certVerifySpkac(object);
}

function exportPublicKey(object, encoding) {
  return certExportPublicKey(toBuf(object, encoding));
}

function exportChallenge(object, encoding) {
  return certExportChallenge(toBuf(object, encoding));
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
