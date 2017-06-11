'use strict';
const common = require('../common');
const assert = require('assert');

const stream = require('stream');
let shutdown = false;

const w = new stream.Writable({
  final: common.mustCall(function(cb) {
    assert.strictEqual(this, w);
    setTimeout(function() {
      shutdown = true;
      cb();
    }, 100);
  }),
  write: function(chunk, e, cb) {
    process.nextTick(cb);
  }
});
w.on('finish', common.mustCall(function() {
  assert(shutdown);
}));
w.write(Buffer.allocUnsafe(1));
w.end(Buffer.allocUnsafe(0));
