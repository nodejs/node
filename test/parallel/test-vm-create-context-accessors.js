'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

let ctx = {};

Object.defineProperty(ctx, 'getter', {
  get: function() {
    return 'ok';
  }
});

let val;
Object.defineProperty(ctx, 'setter', {
  set: function(_val) {
    val = _val;
  },
  get: function() {
    return 'ok=' + val;
  }
});

ctx = vm.createContext(ctx);

const result = vm.runInContext('setter = "test";[getter,setter]', ctx);
assert.strictEqual(result[0], 'ok');
assert.strictEqual(result[1], 'ok=test');
