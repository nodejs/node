'use strict';

// Checks for crash regression: https://github.com/nodejs/node/issues/37430

const common = require('../common');
const assert = require('assert');
const {
  open,
  openSync,
  promises: {
    open: openPromise,
  },
} = require('fs');

// These should throw, not crash.
const invalid = 4_294_967_296;

assert.throws(() => open(__filename, invalid, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => open(__filename, 0, invalid, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => openSync(__filename, invalid), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => openSync(__filename, 0, invalid), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.rejects(openPromise(__filename, invalid), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.rejects(openPromise(__filename, 0, invalid), {
  code: 'ERR_OUT_OF_RANGE'
});
