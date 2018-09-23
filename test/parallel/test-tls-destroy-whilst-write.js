'use strict';
var assert = require('assert');
var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');
var stream = require('stream');

var delay = new stream.Duplex({
  read: function read() {
  },
  write: function write(data, enc, cb) {
    console.log('pending');
    setTimeout(function() {
      console.log('done');
      cb();
    }, 200);
  }
});

var secure = tls.connect({
  socket: delay
});
setImmediate(function() {
  secure.destroy();
});
