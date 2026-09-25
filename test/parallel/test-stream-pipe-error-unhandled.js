'use strict';
const common = require('../common');
const assert = require('assert');
const { Readable, Writable } = require('stream');

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'asd');
}));

const r = new Readable({
  read() {
    this.push('asd');
  }
});
const w = new Writable({
  autoDestroy: true,
  write() {}
});

r.pipe(w);
w.destroy(new Error('asd'));
