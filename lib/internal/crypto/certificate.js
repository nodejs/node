'use strict';

const binding = process.binding('crypto');
const toBuf = require('internal/crypto/util').toBuf;

module.exports = Certificate;

function Certificate() {
  if (!(this instanceof Certificate))
    return new Certificate();
}

Certificate.prototype.verifySpkac = function(object) {
  return binding.certVerifySpkac(object);
};

Certificate.prototype.exportPublicKey = function(object, encoding) {
  return binding.certExportPublicKey(toBuf(object, encoding));
};

Certificate.prototype.exportChallenge = function(object, encoding) {
  return binding.certExportChallenge(toBuf(object, encoding));
};
