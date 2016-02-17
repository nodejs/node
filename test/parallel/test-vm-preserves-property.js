'use strict';

require('../common');
var assert = require('assert');

var vm = require('vm');

var x = {};
Object.defineProperty(x, 'prop', {
  configurable: false,
  enumerable: false,
  writable: false,
  value: 'val'
});
var o = vm.createContext(x);

var code = 'Object.getOwnPropertyDescriptor(this, "prop")';
var res = vm.runInContext(code, o, 'test');

assert(res);
assert.equal(typeof res, 'object');
assert.equal(res.value, 'val');
assert.equal(res.configurable, false, 'should not be configurable');
assert.equal(res.enumerable, false, 'should not be enumerable');
assert.equal(res.writable, false, 'should not be writable');
