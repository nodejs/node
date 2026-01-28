// Flags: --expose-internals
'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { AsyncLocalStorage } = require('async_hooks');
const assert = require('assert');
const AsyncContextFrame = require('internal/async_context_frame');
const { enabledHooksExist } = require('internal/async_hooks');

// This test verifies that ALS context is preserved when using stream.finished()

const als = new AsyncLocalStorage();
const readable = new Readable();

als.run('test-context-1', common.mustCall(() => {
  finished(readable, common.mustCall(() => {
    assert.strictEqual(AsyncContextFrame.enabled || enabledHooksExist(), true);
    assert.strictEqual(als.getStore(), 'test-context-1');
  }));
}));

readable.destroy();
