// Flags: --enable-source-maps

'use strict';
require('../../../common');
const assert = require('node:assert');

// Regression test for https://github.com/nodejs/node/issues/63169
// Under --enable-source-maps with no source map for this file, a failing
// assert.ok(value) must throw AssertionError, not TypeError ERR_INVALID_ARG_TYPE.
assert.ok(false); // eslint-disable-line no-restricted-syntax
