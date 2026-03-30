'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

// Verify that `enterWith()` does not leak the store to the parent context in a promise.

const also = new AsyncLocalStorage();

async function asyncFunctionAfterAwait() {
  await 0;
  also.enterWith('after await');
}

function promiseThen() {
  return Promise.resolve()
    .then(() => {
      also.enterWith('inside then');
    });
}

async function asyncFunctionBeforeAwait() {
  also.enterWith('before await');
  await 0;
}

async function main() {
  await asyncFunctionAfterAwait();
  await promiseThen();
  assert.strictEqual(also.getStore(), undefined);

  // This is a known limitation of the `enterWith` API.
  await asyncFunctionBeforeAwait();
  assert.strictEqual(also.getStore(), 'before await');
}

main().then(common.mustCall());
