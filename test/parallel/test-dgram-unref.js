'use strict';
const common = require('../common');
const assert = require('assert');

const dgram = require('dgram');
var closed = false;

var s = dgram.createSocket('udp4');
s.bind();
s.unref();

setTimeout(function() {
  closed = true;
  s.close();
}, 1000).unref();

process.on('exit', function() {
  assert.strictEqual(closed, false, 'Unrefd socket should not hold loop open');
});
