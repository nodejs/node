'use strict';

const common = require('../common');
const {
  pipeline,
  PassThrough
} = require('stream');
const assert = require('assert');

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'asd');
}));

// Ensure that pipeline that ends with Promise
// still propagates error to uncaughtException.
const s = new PassThrough();
s.end('asd');
pipeline(s, async function(source) {
  for await (const chunk of source) {
    chunk;
  }
}, common.mustCall((err) => {
  assert.ok(!err);
  throw new Error('asd');
}));
