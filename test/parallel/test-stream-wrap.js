'use strict';
const common = require('../common');
const assert = require('assert');

const StreamWrap = require('_stream_wrap');
const Duplex = require('stream').Duplex;
const ShutdownWrap = process.binding('stream_wrap').ShutdownWrap;

var done = false;

function testShutdown(callback) {
  var stream = new Duplex({
    read: function() {
    },
    write: function() {
    }
  });

  var wrap = new StreamWrap(stream);

  var req = new ShutdownWrap();
  req.oncomplete = function(code) {
    assert(code < 0);
    callback();
  };
  req.handle = wrap._handle;

  // Close the handle to simulate
  wrap.destroy();
  req.handle.shutdown(req);
}

testShutdown(function() {
  done = true;
});

process.on('exit', function() {
  assert(done);
});
