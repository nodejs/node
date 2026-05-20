'use strict';
require('../common');
const assert = require('assert');

const nativeProcess = require('process');
// require('process') should return global process reference
assert.strictEqual(nativeProcess, process);
