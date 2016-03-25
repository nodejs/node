'use strict';

const toBuf = require('internal/crypto/util').toBuf;
const printDeprecationMessage =
    require('internal/util').printDeprecationMessage;
const kDeprecationMessage = 'crypto.Certificate is deprecated. ' +
                            'Please use crypto.certVerifySpkac(), ' +
                            'crypto.certExportPublicKey(), and ' +
                            'crypto.certExportChallenge() instead.';

module.exports = function(binding, crypto) {

  crypto.certVerifySpkac = function(object) {
    return binding.certVerifySpkac(object);
  };

  crypto.certExportPublicKey = function(object, encoding) {
    return binding.certExportPublicKey(toBuf(object, encoding));
  };

  crypto.certExportChallenge = function(object, encoding) {
    return binding.certExportChallenge(toBuf(object, encoding));
  };

  var warned;
  function Certificate() {
    if (!(this instanceof Certificate))
      return new Certificate();
    warned = printDeprecationMessage(kDeprecationMessage, warned);
  }
  Certificate.prototype.verifySpkac = crypto.certVerifySpkac;
  Certificate.prototype.exportPublicKey = crypto.certExportPublicKey;
  Certificate.prototype.exportChallenge = crypto.certExportChallenge;

  crypto.Certificate = Certificate;

};
