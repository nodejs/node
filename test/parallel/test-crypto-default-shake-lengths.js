// Flags: --pending-deprecation
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const crypto = require('crypto');
if (!crypto.getHashes().includes('shake128')) {
  common.skip('unsupported shake128 test');
}

const { createHash } = require('crypto');

common.expectWarning({
  DeprecationWarning: {
    DEP0198: 'Creating SHAKE128/256 digests without an explicit options.outputLength is deprecated.',
  }
});

{
  createHash('shake128').update('test').digest();
}
