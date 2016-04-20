'use strict';
require('../common');
var assert = require('assert');
var v8 = require('v8');
var vm = require('vm');

// Note: changing V8 flags after an isolate started is not guaranteed to work.
// Specifically here, V8 may cache compiled scripts between the flip of the
// flag. We use a different script each time to work around this problem.
v8.setFlagsFromString('--allow_natives_syntax');
assert(eval('%_IsSmi(42)'));
assert(vm.runInThisContext('%_IsSmi(43)'));

v8.setFlagsFromString('--noallow_natives_syntax');
assert.throws(function() { eval('%_IsSmi(44)'); }, SyntaxError);
assert.throws(function() { vm.runInThisContext('%_IsSmi(45)'); }, SyntaxError);
