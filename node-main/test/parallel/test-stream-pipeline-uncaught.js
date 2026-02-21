'use strict';

const common = require('../common');
const {
  pipeline,
  PassThrough
} = require('stream');
const assert = require('assert');

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'error');
}));

// Ensure that pipeline that ends with Promise
// still propagates error to uncaughtException.
const s = new PassThrough();
s.end('data');
pipeline(s, async function(source) {
  for await (const chunk of source) { } // eslint-disable-line no-unused-vars, no-empty
}, common.mustSucceed(() => {
  throw new Error('error');
}));
