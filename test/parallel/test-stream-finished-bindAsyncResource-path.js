// Flags: --expose-internals
'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { createHook } = require('async_hooks');
const { strictEqual } = require('assert');
const internalAsyncHooks = require('internal/async_hooks');

// This test verifies that when there are active async hooks, stream.finished() uses
// the bindAsyncResource path

createHook({}).enable();
const readable = new Readable();

finished(readable, common.mustCall(() => {
  strictEqual(internalAsyncHooks.getHookArrays()[0].length > 0,
              true, 'Should have active user async hook');
}));

readable.destroy();
