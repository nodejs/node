'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');

assert.throws(function() { v8.setFlagsFromString(1); }, TypeError);
assert.throws(function() { v8.setFlagsFromString(); }, TypeError);
