'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');
const Buffer = require('buffer').Buffer;

const originalSource = 'function bcd() { return 123; }';

// It should produce code cache
const original = new vm.Script(originalSource, {
  produceCachedData: true
});
assert(original.cachedData instanceof Buffer);

// It should consume code cache
const success = new vm.Script(originalSource, {
  cachedData: original.cachedData
});
assert(!success.cachedDataRejected);

// It should reject invalid code cache
const reject = new vm.Script('function abc() {}', {
  cachedData: original.cachedData
});
assert(reject.cachedDataRejected);

// It should throw on non-Buffer cachedData
assert.throws(() => {
  new vm.Script('function abc() {}', {
    cachedData: 'ohai'
  });
});
