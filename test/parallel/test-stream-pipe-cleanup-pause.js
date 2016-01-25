'use strict';
const common = require('../common');
const stream = require('stream');

const reader = new stream.Readable();
const writer1 = new stream.Writable();
const writer2 = new stream.Writable();

// 560000 is chosen here because it is larger than the (default) highWaterMark
// and will cause `.write()` to return false
// See: https://github.com/nodejs/node/issues/2323
const buffer = Buffer.allocUnsafe(560000);

reader._read = function(n) {};

writer1._write = common.mustCall(function(chunk, encoding, cb) {
  this.emit('chunk-received');
  cb();
}, 1);
writer1.once('chunk-received', function() {
  reader.unpipe(writer1);
  reader.pipe(writer2);
  reader.push(buffer);
  setImmediate(function() {
    reader.push(buffer);
    setImmediate(function() {
      reader.push(buffer);
    });
  });
});

writer2._write = common.mustCall(function(chunk, encoding, cb) {
  cb();
}, 3);

reader.pipe(writer1);
reader.push(buffer);
