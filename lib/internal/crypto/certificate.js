'use strict';

const binding = process.binding('crypto');
const cryptoUtil = require('internal/crypto/util');
const internalUtil = require('internal/util');
const toBuf = cryptoUtil.toBuf;

class Certificate {
  verifySpkac(object) {
    return binding.certVerifySpkac(object);
  }

  exportPublicKey(object, encoding) {
    return binding.certExportPublicKey(toBuf(object, encoding));
  }

  exportChallenge(object, encoding) {
    return binding.certExportChallenge(toBuf(object, encoding));
  }
}

module.exports = {
  Certificate,
  createCertificate: internalUtil.createClassWrapper(Certificate)
};
