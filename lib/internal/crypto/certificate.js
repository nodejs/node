'use strict';

const {
  certExportChallenge,
  certExportPublicKey,
  certVerifySpkac
} = internalBinding('crypto');

const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { isArrayBufferView } = require('internal/util/types');

const {
  toBuf
} = require('internal/crypto/util');

function verifySpkac(spkac) {
  if (!isArrayBufferView(spkac)) {
    throw new ERR_INVALID_ARG_TYPE(
      'spkac',
      ['Buffer', 'TypedArray', 'DataView'],
      spkac
    );
  }
  return certVerifySpkac(spkac);
}

function exportPublicKey(spkac, encoding) {
  spkac = toBuf(spkac, encoding);
  if (!isArrayBufferView(spkac)) {
    throw new ERR_INVALID_ARG_TYPE(
      'spkac',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      spkac
    );
  }
  return certExportPublicKey(spkac);
}

function exportChallenge(spkac, encoding) {
  spkac = toBuf(spkac, encoding);
  if (!isArrayBufferView(spkac)) {
    throw new ERR_INVALID_ARG_TYPE(
      'spkac',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      spkac
    );
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
