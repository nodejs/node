'use strict';
require('../common');
const assert = require('assert');

const vm = require('vm');

const ctx = vm.createContext({});

// Define a non-configurable ("restricted") property on the context global.
vm.runInContext(
  "Object.defineProperty(this, 'foo', { value: 1, configurable: false });",
  ctx
);

// https://tc39.es/ecma262/#sec-globaldeclarationinstantiation 3.d:
// If hasRestrictedGlobal is true, throw a SyntaxError exception.
assert.throws(
  () => vm.runInContext('let foo = 2;', ctx),
  vm.runInContext('SyntaxError', ctx),
);
