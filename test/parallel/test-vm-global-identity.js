'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');

var ctx = vm.createContext();
ctx.window = ctx;

var thisVal = vm.runInContext('this;', ctx);
var windowVal = vm.runInContext('window;', ctx);
assert.strictEqual(thisVal, windowVal);
