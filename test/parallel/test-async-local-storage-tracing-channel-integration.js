'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const contextA = {
  foo: 'bar'
};
const contextB = {
  baz: 'buz'
};

const expected = [
  contextA,
  contextB
];

let builds = 0;
let runs = 0;

const storageA = new AsyncLocalStorage();
const storageB = new AsyncLocalStorage();

// Check both passthrough bindings and transformed bindings
storageA.bindToTracingChannel('test');
storageB.bindToTracingChannel('test', common.mustCall((received) => {
  assert.deepStrictEqual(received, expected[builds++]);
  return { received };
}, 2));

function check(context) {
  assert.deepStrictEqual(storageA.getStore(), context);
  assert.deepStrictEqual(storageB.getStore(), {
    received: context
  });
}

// Should have no context before run
assert.strictEqual(storageA.getStore(), undefined);
assert.strictEqual(storageB.getStore(), undefined);

AsyncLocalStorage.runInTracingChannel('test', expected[runs], common.mustCall(() => {
  // Should have set expected context
  check(expected[runs]);

  // Should support nested contexts
  runs++;
  AsyncLocalStorage.runInTracingChannel('test', expected[runs], common.mustCall(() => {
    check(expected[runs]);
  }));
  runs--;

  // Should have restored outer context
  check(expected[runs]);
}));

// Should have no context after run
assert.strictEqual(storageA.getStore(), undefined);
assert.strictEqual(storageB.getStore(), undefined);
