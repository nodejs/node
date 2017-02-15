'use strict';

const common = require('../common');

const assert = require('assert');
const stream = require('stream');

const writable = new stream.Writable();

writable._write = (chunk, encoding, cb) => {
  assert.strictEqual(writable._writableState.ended, false);
  cb();
};

assert.strictEqual(writable._writableState.ended, false);

writable.end('testing ended state', common.mustCall(() => {
  assert.strictEqual(writable._writableState.ended, true);
}));

assert.strictEqual(writable._writableState.ended, true);
