'use strict';

const common = require('../common');

const assert = require('assert');
const stream = require('stream');

const writable = new stream.Writable();

writable._write = (chunk, encoding, cb) => {
  // The state finished should start in false.
  assert.strictEqual(writable._writableState.finished, false);
  cb();
};

writable.on('finish', common.mustCall(() => {
  assert.strictEqual(writable._writableState.finished, true);
}));

writable.end('testing finished state', common.mustCall(() => {
  assert.strictEqual(writable._writableState.finished, true);
}));
