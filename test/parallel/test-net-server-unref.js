'use strict';
require('../common');
var assert = require('assert');

var net = require('net');
var closed = false;

var s = net.createServer();
s.listen(0);
s.unref();

setTimeout(function() {
  closed = true;
  s.close();
}, 1000).unref();

process.on('exit', function() {
  assert.strictEqual(closed, false, 'Unrefd socket should not hold loop open');
});
