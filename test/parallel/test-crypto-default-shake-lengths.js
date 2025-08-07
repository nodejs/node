'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const crypto = require('crypto');
if (!crypto.getCiphers().includes('shake128')) {
  common.printSkipMessage('unsupported shake128 test');
  return;
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
