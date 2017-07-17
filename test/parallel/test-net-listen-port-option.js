'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

function close() { this.close(); }
net.Server().listen({ port: undefined }, close);
net.Server().listen({ port: '0' }, close);

[ 'nan',
  -1,
  123.456,
  0x10000,
  1 / 0,
  -1 / 0,
  '+Infinity',
  '-Infinity'
].forEach(function(port) {
  assert.throws(function() {
    net.Server().listen({ port: port }, common.fail);
  }, /port should be >= 0 and < 65536/i);
});

[null, true, false].forEach(function(port) {
  assert.throws(function() {
    net.Server().listen({ port: port }, common.fail);
  }, /invalid listen argument/i);
});

// Repeat the tests, passing port as an argument, which validates somewhat
// differently.

net.Server().listen(undefined, close);
net.Server().listen('0', close);

// 'nan', skip, treated as a path, not a port
//'+Infinity', skip, treated as a path, not a port
//'-Infinity' skip, treated as a path, not a port

// The numbers that are out of range are treated as 0
net.Server().listen(-1, close);
// Use a float larger than 1024, because EACCESS happens for low ports
net.Server().listen(1234.56, close);
net.Server().listen(0x10000, close);
net.Server().listen(1 / 0, close);
net.Server().listen(-1 / 0, close);

// null is treated as 0
net.Server().listen(null, close);

// false/true are converted to 0/1, arguably a bug, but fixing would be
// semver-major. Note that true fails because ports that low can't be listened
// on by unprivileged processes.
net.Server().listen(false, close);

net.Server().listen(true).on('error', common.mustCall(function(err) {
  assert.strictEqual(err.code, 'EACCES');
}));
