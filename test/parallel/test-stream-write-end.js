'use strict';
require('../common');
var assert = require('assert');

var stream = require('stream');
var w = new stream.Writable({
  end: function(cb) {
    assert(this === w);
    setTimeout(function() {
      shutdown = true;
      cb();
    }, 100);
  },
  write:  function(chunk, e, cb) {
    process.nextTick(cb);
  }
});
var shutdown = false;
w.on('finish', function() {
  assert(shutdown);
});
w.write(Buffer(1));
w.end(Buffer(0));
