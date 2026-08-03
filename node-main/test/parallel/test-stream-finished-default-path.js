// Flags: --expose-internals --no-async-context-frame
'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { strictEqual } = require('assert');
const AsyncContextFrame = require('internal/async_context_frame');
const internalAsyncHooks = require('internal/async_hooks');

// This test verifies that when there are no active async hooks, stream.finished() uses the default callback path

const readable = new Readable();

finished(readable, common.mustCall(() => {
  strictEqual(internalAsyncHooks.getHookArrays()[0].length === 0,
              true, 'Should not have active user async hook');
  strictEqual(AsyncContextFrame.current() || internalAsyncHooks.getHookArrays()[0].length > 0,
              false, 'Default callback path should be used');
}));

readable.destroy();
