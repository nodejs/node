'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.features.openssl_is_boringssl) {
  common.skip('Skipping unsupported shake128 digest method test');
}

const { hash } = require('crypto');

common.expectWarning({
  DeprecationWarning: {
    DEP0198: 'Creating SHAKE128/256 digests without an explicit options.outputLength is deprecated.',
  }
});

{
  hash('shake128', 'test');
}
