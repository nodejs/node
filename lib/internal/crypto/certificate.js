'use strict';

const {
  certExportChallenge,
  certExportPublicKey,
  certVerifySpkac
} = process.binding('crypto');

const errors = require('internal/errors');
const { isArrayBufferView } = require('internal/util/types');

const {
  toBuf
} = require('internal/crypto/util');

function verifySpkac(spkac) {
  if (!isArrayBufferView(spkac)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'spkac',
                               ['Buffer', 'TypedArray', 'DataView']);
  }
  return certVerifySpkac(spkac);
}

function exportPublicKey(spkac, encoding) {
  spkac = toBuf(spkac, encoding);
  if (!isArrayBufferView(spkac)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'spkac',
                               ['string', 'Buffer', 'TypedArray', 'DataView']);
  }
  return certExportPublicKey(spkac);
}

function exportChallenge(spkac, encoding) {
  spkac = toBuf(spkac, encoding);
  if (!isArrayBufferView(spkac)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'spkac',
                               ['string', 'Buffer', 'TypedArray', 'DataView']);
  }
  return certExportChallenge(spkac);
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
