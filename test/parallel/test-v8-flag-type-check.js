'use strict';
require('../common');
var assert = require('assert');
var v8 = require('v8');

assert.throws(function() {v8.setFlagsFromString(1);}, TypeError);
assert.throws(function() {v8.setFlagsFromString();}, TypeError);
