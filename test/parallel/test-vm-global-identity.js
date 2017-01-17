'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const ctx = vm.createContext();
ctx.window = ctx;

const thisVal = vm.runInContext('this;', ctx);
const windowVal = vm.runInContext('window;', ctx);
assert.strictEqual(thisVal, windowVal);
