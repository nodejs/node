'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');
const vm = require('vm');

v8.setFlagsFromString('--allow_natives_syntax');
assert(eval('%_IsSmi(42)'));
assert(vm.runInThisContext('%_IsSmi(42)'));

v8.setFlagsFromString('--noallow_natives_syntax');
assert.throws(function() { eval('%_IsSmi(42)'); }, SyntaxError);
assert.throws(function() { vm.runInThisContext('%_IsSmi(42)'); }, SyntaxError);
