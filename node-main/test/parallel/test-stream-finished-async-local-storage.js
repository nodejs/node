// Flags: --expose-internals
'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { AsyncLocalStorage } = require('async_hooks');
const { strictEqual } = require('assert');
const AsyncContextFrame = require('internal/async_context_frame');
const internalAsyncHooks = require('internal/async_hooks');

// This test verifies that ALS context is preserved when using stream.finished()

const als = new AsyncLocalStorage();
const readable = new Readable();

als.run('test-context-1', () => {
  finished(readable, common.mustCall(() => {
    strictEqual(AsyncContextFrame.enabled || internalAsyncHooks.getHookArrays()[0].length > 0,
                true, 'One of AsyncContextFrame or async hooks criteria should be met');
    strictEqual(als.getStore(), 'test-context-1', 'ALS context should be preserved');
  }));
});

readable.destroy();
