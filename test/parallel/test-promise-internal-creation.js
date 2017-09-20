'use strict';
const common = require('../common');
const assert = require('assert');
const {
  createPromise, promiseResolve, promiseReject
} = process.binding('util');
const { inspect } = require('util');

common.crashOnUnhandledRejection();

{
  const promise = createPromise();
  assert.strictEqual(inspect(promise), 'Promise { <pending> }');
  promiseResolve(promise, 42);
  assert.strictEqual(inspect(promise), 'Promise { 42 }');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 42);
  }));
}

{
  const promise = createPromise();
  const error = new Error('foobar');
  promiseReject(promise, error);
  assert(inspect(promise).includes('<rejected> Error: foobar'));
  promise.catch(common.mustCall((value) => {
    assert.strictEqual(value, error);
  }));
}

{
  const promise = createPromise();
  const error = new Error('foobar');
  promiseReject(promise, error);
  assert(inspect(promise).includes('<rejected> Error: foobar'));
  promiseResolve(promise, 42);
  assert(inspect(promise).includes('<rejected> Error: foobar'));
  promise.catch(common.mustCall((value) => {
    assert.strictEqual(value, error);
  }));
}
