// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('internal/util');

const expectedError = common.expectsError({
  code: 'ERR_NO_CRYPTO',
  type: Error
});

if (!process.versions.openssl) {
  assert.throws(() => util.assertCrypto(), expectedError);
} else {
  assert.doesNotThrow(() => util.assertCrypto());
}
