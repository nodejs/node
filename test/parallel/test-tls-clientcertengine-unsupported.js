'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

// Monkey-patch SecureContext
const binding = process.binding('crypto');
const NativeSecureContext = binding.SecureContext;

binding.SecureContext = function() {
  const rv = new NativeSecureContext();
  rv.setClientCertEngine = undefined;
  return rv;
};

const assert = require('assert');
const tls = require('tls');

{
  assert.throws(
    () => { tls.createSecureContext({ clientCertEngine: 'Cannonmouth' }); },
    common.expectsError({
      code: 'ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED',
      message: 'Custom engines not supported by this OpenSSL'
    })
  );
}
