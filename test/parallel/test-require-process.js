'use strict';
require('../common');
const assert = require('assert');

const nativeProcess = require('process');
assert.strictEqual(nativeProcess, process,
                   'require("process") should return global process reference');
