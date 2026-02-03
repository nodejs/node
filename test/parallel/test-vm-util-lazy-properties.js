'use strict';
require('../common');

const vm = require('node:vm');
const util = require('node:util');
const assert = require('node:assert');

// This verifies that invoking property getters defined with
// `require('internal/util').defineLazyProperties` does not crash
// the process.

const ctx = vm.createContext();
const getter = vm.runInContext(`
  function getter(object, property) {
    return object[property];
  }
  getter;
`, ctx);

// `util.parseArgs` is a lazy property.
const parseArgs = getter(util, 'parseArgs');
assert.strictEqual(parseArgs, util.parseArgs);

// `globalThis.TextEncoder` is a lazy property.
const TextEncoder = getter(globalThis, 'TextEncoder');
assert.strictEqual(TextEncoder, globalThis.TextEncoder);
