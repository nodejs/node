'use strict';
const common = require('../common');
const assert = require('assert');

const StreamWrap = require('_stream_wrap');
const Duplex = require('stream').Duplex;
const ShutdownWrap = process.binding('stream_wrap').ShutdownWrap;

function testShutdown(callback) {
  var stream = new Duplex({
    read: function() {
    },
    write: function(data, enc, callback) {
      callback(null);
    }
  });

  var wrap = new StreamWrap(stream);

  var req = new ShutdownWrap();
  req.oncomplete = function() {};
  req.handle = wrap._handle;
  wrap._handle.shutdown(req);

  wrap.destroy();

  process.nextTick(callback);
}

function testReadAfterClose(callback) {
  var stream = new Duplex({
    read: function() {
    },
    write: function(data, enc, callback) {
      callback(null);
    }
  });
  stream.push('data');
  stream.push(null);

  var wrap = new StreamWrap(stream);
}

testShutdown(function() {
  testReadAfterClose();
});
