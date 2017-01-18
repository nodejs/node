'use strict';
require('../common');
const stream = require('stream');
const Buffer = require('buffer').Buffer;

const r = new stream.Readable();
r._read = function(size) {
  r.push(Buffer.allocUnsafe(size));
};

const w = new stream.Writable();
w._write = function(data, encoding, cb) {
  cb(null);
};

r.pipe(w);

// This might sound unrealistic, but it happens in net.js. When
// `socket.allowHalfOpen === false`, EOF will cause `.destroySoon()` call which
// ends the writable side of net.Socket.
w.end();
