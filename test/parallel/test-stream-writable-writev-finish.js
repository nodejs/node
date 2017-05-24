'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

// ensure consistency between the finish event when using cork()
// and writev and when not using them

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    cb(new Error('write test error'));
  };

  writable.on('finish', common.mustCall());

  writable.on('prefinish', common.mustCall());

  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'write test error');
  }));

  writable.end('test');
}

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    cb(new Error('write test error'));
  };

  writable._writev = (chunks, cb) => {
    cb(new Error('writev test error'));
  };

  writable.on('finish', common.mustCall());

  writable.on('prefinish', common.mustCall());

  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'writev test error');
  }));

  writable.cork();
  writable.write('test');

  setImmediate(function() {
    writable.end('test');
  });
}
