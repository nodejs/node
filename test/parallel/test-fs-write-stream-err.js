'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

common.refreshTmpDir();

var stream = fs.createWriteStream(common.tmpDir + '/out', {
  highWaterMark: 10
});
var err = new Error('BAM');

var write = fs.write;
var writeCalls = 0;
fs.write = function() {
  switch (writeCalls++) {
    case 0:
      console.error('first write');
      // first time is ok.
      return write.apply(fs, arguments);
    case 1:
      // then it breaks
      console.error('second write');
      var cb = arguments[arguments.length - 1];
      return process.nextTick(function() {
        cb(err);
      });
    default:
      // and should not be called again!
      throw new Error('BOOM!');
  }
};

fs.close = common.mustCall(function(fd_, cb) {
  console.error('fs.close', fd_, stream.fd);
  assert.equal(fd_, stream.fd);
  process.nextTick(cb);
});

stream.on('error', common.mustCall(function(err_) {
  console.error('error handler');
  assert.equal(stream.fd, null);
  assert.equal(err_, err);
}));


stream.write(Buffer.allocUnsafe(256), function() {
  console.error('first cb');
  stream.write(Buffer.allocUnsafe(256), common.mustCall(function(err_) {
    console.error('second cb');
    assert.equal(err_, err);
  }));
});
