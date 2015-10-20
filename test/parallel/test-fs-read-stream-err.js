'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

var stream = fs.createReadStream(__filename, {
  bufferSize: 64
});
var err = new Error('BAM');

stream.on('error', common.mustCall(function errorHandler(err_) {
  console.error('error event');
  process.nextTick(function() {
    assert.equal(stream.fd, null);
    assert.equal(err_, err);
  });
}));

fs.close = common.mustCall(function(fd_, cb) {
  assert.equal(fd_, stream.fd);
  process.nextTick(cb);
});

var read = fs.read;
fs.read = function() {
  // first time is ok.
  read.apply(fs, arguments);
  // then it breaks
  fs.read = function() {
    var cb = arguments[arguments.length - 1];
    process.nextTick(function() {
      cb(err);
    });
    // and should not be called again!
    fs.read = function() {
      throw new Error('BOOM!');
    };
  };
};

stream.on('data', function(buf) {
  stream.on('data', common.fail);  // no more 'data' events should follow
});
