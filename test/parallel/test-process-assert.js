'use strict';
const common = require('../common');
const assert = require('assert');

assert.strictEqual(process.assert(1, 'error'), undefined);
common.expectsError(() => {
  process.assert(undefined, 'errorMessage');
}, {
  code: 'ERR_ASSERTION',
  type: Error,
  message: 'errorMessage'
}
);
common.expectsError(() => {
  process.assert(false);
}, {
  code: 'ERR_ASSERTION',
  type: Error,
  message: 'assertion error'
}
);
