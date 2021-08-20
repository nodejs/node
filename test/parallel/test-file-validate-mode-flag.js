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

assert.throws(() => open(__filename, 2176057344, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => open(__filename, 0, 2176057344, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => openSync(__filename, 2176057344), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => openSync(__filename, 0, 2176057344), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.rejects(openPromise(__filename, 2176057344), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.rejects(openPromise(__filename, 0, 2176057344), {
  code: 'ERR_OUT_OF_RANGE'
});
