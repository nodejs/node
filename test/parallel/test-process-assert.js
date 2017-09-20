'use strict';
const common = require('../common');
const assert = require('assert');

assert.strictEqual(process.assert(1, 'error'), undefined);
assert.throws(() => {
  process.assert(undefined, 'errorMessage');
}, common.expectsError({
  code: 'ERR_ASSERTION',
  type: Error,
  message: 'errorMessage'
})
);
assert.throws(() => {
  process.assert(false);
}, common.expectsError({
  code: 'ERR_ASSERTION',
  type: Error,
  message: 'assertion error'
})
);
