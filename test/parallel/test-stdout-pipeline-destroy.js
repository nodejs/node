'use strict';

const common = require('../common');
const { Transform, Readable, pipeline } = require('stream');
const assert = require('assert');

const reader = new Readable({
  read(size) { this.push('foo'); }
});

let count = 0;

const err = new Error('this-error-gets-hidden');

const transform = new Transform({
  transform(chunk, enc, cb) {
    if (count++ >= 5)
      this.emit('error', err);
    else
      cb(null, count.toString() + '\n');
  }
});

pipeline(
  reader,
  transform,
  process.stdout,
  common.mustCall((e) => {
    assert.strictEqual(e, err);
  })
);
