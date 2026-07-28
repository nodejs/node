'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!process.features.openssl_is_boringssl)
  common.skip('only applies to BoringSSL builds');

const assert = require('assert');
const tls = require('tls');

// BoringSSL does not provide SSL_CTX_set_dh_auto, so requesting automatic
// DH parameter selection via `dhparam: 'auto'` must throw.
assert.throws(() => {
  tls.createSecureContext({ dhparam: 'auto' });
}, {
  code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION',
  message: 'Automatic DH parameter selection is not supported',
});
