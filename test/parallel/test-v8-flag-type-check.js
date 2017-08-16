'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');

assert.throws(function() { v8.setFlagsFromString(1); },
              /^TypeError: v8 flag must be a string$/);
assert.throws(function() { v8.setFlagsFromString(); },
              /^TypeError: v8 flag is required$/);
