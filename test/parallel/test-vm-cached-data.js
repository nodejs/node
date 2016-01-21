'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');
const Buffer = require('buffer').Buffer;

const originalSource = '(function bcd() { return \'original\'; })';

// It should produce code cache
const original = new vm.Script(originalSource, {
  produceCachedData: true
});
assert(original.cachedData instanceof Buffer);

assert.equal(original.runInThisContext()(), 'original');

// It should consume code cache
const success = new vm.Script(originalSource, {
  cachedData: original.cachedData
});
assert(!success.cachedDataRejected);

assert.equal(success.runInThisContext()(), 'original');

// It should reject invalid code cache
const reject = new vm.Script('(function abc() { return \'invalid\'; })', {
  cachedData: original.cachedData
});
assert(reject.cachedDataRejected);
assert.equal(reject.runInThisContext()(), 'invalid');

// It should throw on non-Buffer cachedData
assert.throws(() => {
  new vm.Script('function abc() {}', {
    cachedData: 'ohai'
  });
});
