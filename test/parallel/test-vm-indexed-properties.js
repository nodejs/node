'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

const code = `Object.defineProperty(this, 99, {
      value: 20,
      enumerable: true
 });`;


const sandbox = {};
const ctx = vm.createContext(sandbox);
vm.runInContext(code, ctx);

assert.strictEqual(sandbox[99], 20);
