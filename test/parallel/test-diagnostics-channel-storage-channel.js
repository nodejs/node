'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const { AsyncLocalStorage } = require('async_hooks');
const assert = require('assert');

const store = new AsyncLocalStorage();

const input = {
  foo: 'bar'
};

const output = {
  baz: 'buz'
};

const channel = dc.storageChannel('test');

// Should only be called once to verify unbind works
const build = common.mustCall((foundInput) => {
  assert.deepStrictEqual(foundInput, input);
  return output;
});

channel.bindStore(store, build);

// Should have an empty store before run
assert.strictEqual(store.getStore(), undefined);

// Should return the result of its runner function
const result = channel.run(input, common.mustCall(() => {
  // Should have a store containing the result of the build function
  assert.deepStrictEqual(store.getStore(), output);
  return output;
}));
assert.deepStrictEqual(result, output);

// Should have an empty store after run
assert.strictEqual(store.getStore(), undefined);

// Should have an empty store during run after unbinding
channel.unbindStore(store, build);
channel.run(input, common.mustCall(function(arg) {
  assert.deepStrictEqual(this, input);
  assert.deepStrictEqual(arg, output);
  assert.strictEqual(store.getStore(), undefined);
}), input, output);

// Should allow omitting build on bind
const a = dc.storageChannel('test');
a.bindStore(store);

// Should allow nested runs
const b = dc.storageChannel('test');
b.run(1, common.mustCall(() => {
  assert.strictEqual(store.getStore(), 1);
  b.run(2, common.mustCall(() => {
    assert.strictEqual(store.getStore(), 2);
  }));
  assert.strictEqual(store.getStore(), 1);
}));
