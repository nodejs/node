'use strict';
const assert = require('assert');

const nativeProcess = require('process');
assert.strictEqual(nativeProcess, process,
  'require("process") should return a reference to global process');
