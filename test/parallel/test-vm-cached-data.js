'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');
const Buffer = require('buffer').Buffer;

function getSource(tag) {
  return `(function ${tag}() { return \'${tag}\'; })`;
}

function produce(source) {
  const script = new vm.Script(source, {
    produceCachedData: true
  });
  assert(!script.cachedDataProduced || script.cachedData instanceof Buffer);

  return script.cachedData;
}

function testProduceConsume() {
  const source = getSource('original');

  const data = produce(source);

  // It should consume code cache
  const script = new vm.Script(source, {
    cachedData: data
  });
  assert(!script.cachedDataRejected);
  assert.equal(script.runInThisContext()(), 'original');
}
testProduceConsume();

function testProduceMultiple() {
  const source = getSource('original');

  produce(source);
  produce(source);
  produce(source);
}
testProduceMultiple();

function testRejectInvalid() {
  const source = getSource('invalid');

  const data = produce(source);

  // It should reject invalid code cache
  const script = new vm.Script(getSource('invalid_1'), {
    cachedData: data
  });
  assert(script.cachedDataRejected);
  assert.equal(script.runInThisContext()(), 'invalid_1');
}
testRejectInvalid();

function testRejectSlice() {
  const source = getSource('slice');

  const data = produce(source).slice(4);

  const script = new vm.Script(source, {
    cachedData: data
  });
  assert(script.cachedDataRejected);
}
testRejectSlice();

// It should throw on non-Buffer cachedData
assert.throws(() => {
  new vm.Script('function abc() {}', {
    cachedData: 'ohai'
  });
});
