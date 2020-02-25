'use strict';
require('../common');
const assert = require('assert');
const {
  AsyncLocalStorage,
  executionAsyncResource
} = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

const outerResource = executionAsyncResource();

asyncLocalStorage.run(new Map(), () => {
  assert.notStrictEqual(executionAsyncResource(), outerResource);
});

assert.strictEqual(executionAsyncResource(), outerResource);
