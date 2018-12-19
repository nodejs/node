'use strict';
const common = require('../common');

common.exposeInternals();

const assert = require('assert');
const util = require('internal/util');

if (!process.versions.openssl) {
  const expectedError = common.expectsError({
    code: 'ERR_NO_CRYPTO',
    type: Error
  });
  assert.throws(() => util.assertCrypto(), expectedError);
} else {
  util.assertCrypto();
}
