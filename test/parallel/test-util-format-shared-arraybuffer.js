'use strict';
require('../common');
const assert = require('assert');
const util = require('util');
const sab = new SharedArrayBuffer(4);
assert.strictEqual(util.format(sab), 'SharedArrayBuffer { byteLength: 4 }');
