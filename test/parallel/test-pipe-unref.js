'use strict';
var common = require('../common');
var assert = require('assert');

var net = require('net');
var closed = false;

common.refreshTmpDir();

var s = net.Server();
s.listen(common.PIPE);
s.unref();

setTimeout(function() {
  closed = true;
  s.close();
}, 1000).unref();

process.on('exit', function() {
  assert.strictEqual(closed, false, 'Unrefd socket should not hold loop open');
});
