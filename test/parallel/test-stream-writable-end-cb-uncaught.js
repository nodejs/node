'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'kaboom');
}));

const writable = new stream.Writable();

writable._write = (chunk, encoding, cb) => {
  cb();
};
writable._final = (cb) => {
  cb(new Error('kaboom'));
};

writable.write('asd');
writable.end(common.mustCall((err) => {
  assert.strictEqual(err.message, 'kaboom');
}));
