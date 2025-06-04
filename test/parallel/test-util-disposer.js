'use strict';

// This test checks that the semantics of `util.disposer` are as described in
// the API docs

const common = require('../common');
const assert = require('node:assert');
const { disposer } = require('node:util');
const test = require('node:test');

test('util.disposer should throw on non-function first parameter', () => {
  const invalidDisposers = [
    null,
    undefined,
    42,
    'string',
    {},
    [],
    Symbol('symbol'),
  ];
  for (const invalidDisposer of invalidDisposers) {
    assert.throws(() => {
      disposer(invalidDisposer);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
  }
});

test('util.disposer should create a Disposable object', () => {
  const disposeFn = common.mustCall();
  const disposable = disposer(disposeFn);
  assert.strictEqual(typeof disposable, 'object');
  assert.strictEqual(typeof disposable[Symbol.dispose], 'function');
  assert.strictEqual(disposable[Symbol.asyncDispose], undefined);
  disposable[Symbol.dispose]();
  // Multiple calls to dispose should not throw and only invoke the function once.
  disposable[Symbol.dispose]();
});

test('Disposer[Symbol.dispose] must be invoked with an Disposer', () => {
  const disposeFn = common.mustNotCall();
  const disposable = disposer(disposeFn);
  assert.throws(() => {
    disposable[Symbol.dispose].call({}); // Call with a non-Disposer object
  }, TypeError);

  assert.throws(() => {
    disposable.dispose.call({}); // Call with a non-Disposer object
  }, TypeError);
});

test('Disposer[Symbol.dispose] should throw if the disposerFn throws', () => {
  const disposeFn = common.mustCall(() => {
    throw new Error('Disposer error');
  });
  const disposable = disposer(disposeFn);
  assert.throws(() => {
    disposable[Symbol.dispose]();
  }, {
    name: 'Error',
    message: 'Disposer error',
  });

  // Multiple calls to dispose should not throw and only invoke the function once.
  disposable[Symbol.dispose]();
});
