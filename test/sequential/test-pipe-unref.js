'use strict';
const common = require('../common');
const assert = require('assert');

const net = require('net');
const closed = false;

const s = net.Server();
s.listen(common.PIPE);
s.unref();

setTimeout(function() {
  closed = true;
  s.close();
}, 1000).unref();

process.on('exit', function() {
  assert.strictEqual(closed, false, 'Unrefd socket should not hold loop open');
});
