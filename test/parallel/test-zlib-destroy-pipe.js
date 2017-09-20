'use strict';

const common = require('../common');
const zlib = require('zlib');
const { Writable } = require('stream');

// verify that the zlib transform does not error in case
// it is destroyed with data still in flight

const ts = zlib.createGzip();

const ws = new Writable({
  write: common.mustCall((chunk, enc, cb) => {
    setImmediate(cb);
    ts.destroy();
  })
});

const buf = Buffer.allocUnsafe(1024 * 1024 * 20);
ts.end(buf);
ts.pipe(ws);
