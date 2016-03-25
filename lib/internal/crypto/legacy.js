'use strict';

const internalUtil = require('internal/util');

module.exports = function(crypto) {

  Object.defineProperty(crypto, 'createCredentials', {
    configurable: false,
    value: internalUtil.deprecate(() => {
      return require('tls').createSecureContext;
    }, 'crypto.createCredentials is deprecated. ' +
       'Use tls.createSecureContext instead.')
  });

  Object.defineProperty(crypto, 'Credentials', {
    configurable: false,
    value: internalUtil.deprecate(() => {
      return require('tls').SecureContext;
    }, 'crypto.Credentials is deprecated. ' +
       'Use tls.createSecureContext instead.')
  });

};
