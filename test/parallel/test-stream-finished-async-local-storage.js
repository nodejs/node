'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { AsyncLocalStorage } = require('async_hooks');
const assert = require('assert');

// This test verifies that ALS context is preserved when using stream.finished()

const als = new AsyncLocalStorage();
const readable = new Readable();

als.run('test-context-1', common.mustCall(() => {
  finished(readable, common.mustCall(() => {
    assert.strictEqual(als.getStore(), 'test-context-1');
  }));
}));

readable.destroy();
