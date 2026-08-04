'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

function a() {
  try {
    return a();
  } catch {
    // Throw an exception as near to the recursion-based RangeError as possible.
    return vm.runInThisContext('() => 42')();
  }
}

assert.strictEqual(a(), 42);

function b() {
  try {
    return b();
  } catch {
    // This writes a lot of noise to stderr, but it still works.
    return vm.runInNewContext('() => 42')();
  }
}

assert.strictEqual(b(), 42);
