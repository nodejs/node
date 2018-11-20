var common = require('../common');
var assert = require('assert');
var v8 = require('v8');
var vm = require('vm');

v8.setFlagsFromString('--allow_natives_syntax');
assert(eval('%_IsSmi(42)'));
assert(vm.runInThisContext('%_IsSmi(42)'));

v8.setFlagsFromString('--noallow_natives_syntax');
assert.throws(function() { eval('%_IsSmi(42)') }, SyntaxError);
assert.throws(function() { vm.runInThisContext('%_IsSmi(42)') }, SyntaxError);
