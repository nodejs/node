'use strict';
const common = require('../common');
const assert = require('assert');

const StreamWrap = require('_stream_wrap');
const Duplex = require('stream').Duplex;
const ShutdownWrap = process.binding('stream_wrap').ShutdownWrap;

function testShutdown(callback) {
  const stream = new Duplex({
    read: function() {
    },
    write: function() {
    }
  });

  const wrap = new StreamWrap(stream);

  const req = new ShutdownWrap();
  req.oncomplete = function(code) {
    assert(code < 0);
    callback();
  };
  req.handle = wrap._handle;

  // Close the handle to simulate
  wrap.destroy();
  req.handle.shutdown(req);
}

testShutdown(common.mustCall());
