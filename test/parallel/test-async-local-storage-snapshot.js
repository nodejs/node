'use strict';

const common = require('../common');
const { strictEqual } = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
const runInAsyncScope =
  asyncLocalStorage.run(123, common.mustCall(() => AsyncLocalStorage.snapshot()));
const result =
  asyncLocalStorage.run(321, common.mustCall(() => {
    return runInAsyncScope(() => {
      return asyncLocalStorage.getStore();
    });
  }));
strictEqual(result, 123);
