'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var binding = process.binding('fs');

common.refreshTmpDir();

var stream = fs.createWriteStream(common.tmpDir + '/out', {
  highWaterMark: 10
});
var err = new Error('BAM');

var writeBuffer = binding.writeBuffer;
var writeCalls = 0;
binding.writeBuffer = function() {
  switch (writeCalls++) {
    case 0:
      // first time is ok.
      console.error('first write');
      return writeBuffer.apply(this, arguments);
    case 1:
      // then it breaks
      console.error('second write');
      return process.nextTick(arguments[5].oncomplete, err);
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


stream.write(new Buffer(256), function() {
  console.error('first cb');
  stream.write(new Buffer(256), common.mustCall(function(err_) {
    console.error('second cb');
    assert.equal(err_, err);
  }));
});
