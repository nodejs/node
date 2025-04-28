'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const { AsyncLocalStorage } = require('async_hooks');
const { strictEqual } = require('assert');

// This test verifies that AsyncLocalStorage context is maintained
// when using stream.finished()

const readable = new Readable();
const als = new AsyncLocalStorage();

als.run(321, () => {
  finished(readable, common.mustCall(() => {
    strictEqual(als.getStore(), 321);
  }));
});

readable.destroy();
