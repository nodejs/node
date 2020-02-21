'use strict';
const common = require('../common');
const stream = require('stream');

process.on('uncaughtException', common.mustCall());

const r = new stream.Readable();
r._read = function(size) {
  r.push(Buffer.allocUnsafe(size));
};

const w = new stream.Writable();
w._write = function(data, encoding, cb) {
  cb(null);
};

r.pipe(w);

// end() after pipe should cause unhandled exception
w.end();
