// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('internal/util');

if (!process.versions.openssl) {
  const expectedError = common.expectsError({
    code: 'ERR_NO_CRYPTO',
    name: 'Error'
  });
  assert.throws(() => util.assertCrypto(), expectedError);
} else {
  util.assertCrypto();
}
