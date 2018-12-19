// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const StreamWrap = require('internal/js_stream_socket');
const { Duplex } = require('stream');
const { ShutdownWrap } = internalBinding('stream_wrap');

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
