// Flags: --expose-brotli
'use strict';

const common = require('../common');
const brotli = require('brotli');
const { Writable } = require('stream');

// verify that the brotli transform does not error in case
// it is destroyed with data still in flight

const ts = new brotli.Compress();

const ws = new Writable({
  write: common.mustCall((chunk, enc, cb) => {
    setImmediate(cb);
    ts.destroy();
  })
});

const buf = Buffer.allocUnsafe(1024 * 1024 * 20);
ts.end(buf);
ts.pipe(ws);
