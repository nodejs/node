'use strict';

// This test checks that the semantics of `util.asyncDisposer` are as described in
// the API docs

const common = require('../common');
const assert = require('node:assert');
const { asyncDisposer } = require('node:util');
const test = require('node:test');

test('util.asyncDisposer should throw on non-function first parameter', () => {
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
      asyncDisposer(invalidDisposer);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
  }
});

test('util.asyncDisposer should create a AsyncDisposable object', async () => {
  const disposeFn = common.mustCall();
  const disposable = asyncDisposer(disposeFn);
  assert.strictEqual(typeof disposable, 'object');
  assert.strictEqual(disposable[Symbol.dispose], undefined);
  assert.strictEqual(typeof disposable[Symbol.asyncDispose], 'function');

  // Multiple calls to asyncDispose should not throw and only invoke the function once.
  const p1 = disposable[Symbol.asyncDispose]();
  const p2 = disposable[Symbol.asyncDispose]();
  assert.strictEqual(p1, p2);
  await p1;
});

test('AsyncDisposer[Symbol.asyncDispose] must be invoked with an AsyncDisposer', () => {
  const disposeFn = common.mustNotCall();
  const disposable = asyncDisposer(disposeFn);
  assert.throws(() => {
    disposable[Symbol.asyncDispose].call({}); // Call with a non-AsyncDisposer object
  }, TypeError);

  assert.throws(() => {
    disposable.dispose.call({}); // Call with a non-AsyncDisposer object
  }, TypeError);
});

test('AsyncDisposer[Symbol.asyncDispose] should reject if the disposerFn throws sync', async () => {
  const disposeFn = common.mustCall(() => {
    throw new Error('Disposer error');
  });
  const disposable = asyncDisposer(disposeFn);
  const promise = disposable[Symbol.asyncDispose]();

  await assert.rejects(promise, {
    name: 'Error',
    message: 'Disposer error',
  });
});

test('Disposer[Symbol.asyncDispose] should reject if the disposerFn rejects', async () => {
  const disposeFn = common.mustCall(() => {
    return Promise.reject(new Error('Disposer error'));
  });
  const disposable = asyncDisposer(disposeFn);
  const promise = disposable[Symbol.asyncDispose]();

  await assert.rejects(promise, {
    name: 'Error',
    message: 'Disposer error',
  });
});
