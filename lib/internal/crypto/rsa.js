'use strict';

const constants = require('constants');
const toBuf = require('internal/crypto/util').toBuf;

function rsa(method, defaultPadding) {
  return function(options, buffer) {
    return method(toBuf(options.key || options),
                  buffer,
                  options.padding || defaultPadding,
                  options.passphrase || null);
  };
}

module.exports = function(binding, crypto) {
  crypto.publicEncrypt = rsa(binding.publicEncrypt,
                             constants.RSA_PKCS1_OAEP_PADDING);
  crypto.publicDecrypt = rsa(binding.publicDecrypt,
                             constants.RSA_PKCS1_PADDING);
  crypto.privateEncrypt = rsa(binding.privateEncrypt,
                              constants.RSA_PKCS1_PADDING);
  crypto.privateDecrypt = rsa(binding.privateDecrypt,
                              constants.RSA_PKCS1_OAEP_PADDING);
};
