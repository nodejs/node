'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

// Verify that `enterWith()` does not leak the store to the parent context in a promise.

const als = new AsyncLocalStorage();

async function asyncFunctionAfterAwait() {
  await 0;
  als.enterWith('after await');
}

function promiseThen() {
  return Promise.resolve()
    .then(() => {
      als.enterWith('inside then');
    });
}

async function asyncFunctionBeforeAwait() {
  als.enterWith('before await');
  await 0;
}

async function main() {
  await asyncFunctionAfterAwait();
  await promiseThen();
  assert.strictEqual(als.getStore(), undefined);

  // This is a known limitation of the `enterWith` API.
  await asyncFunctionBeforeAwait();
  assert.strictEqual(als.getStore(), 'before await');
}

main().then(common.mustCall());
