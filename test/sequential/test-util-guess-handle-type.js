'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');
const fs = require('fs');
const path = require('path');
const net = require('net');

// Throw on non-numeric fd
assert.throws(function() {
  util.guessHandleType('test');
}, /fd must be a number/);

// Throw on fd < 0
assert.throws(function() {
  util.guessHandleType(-1);
}, /fd cannot be less than 0/);

// Check for FILE handle type
const filename = path.join(common.tmpDir, 'guess-handle');
common.refreshTmpDir();
fs.writeFileSync(filename, '', 'utf8');
const fd = fs.openSync(filename, 'r+');
assert.strictEqual(util.guessHandleType(fd), 'FILE');
fs.closeSync(fd);
fs.unlinkSync(filename);

// Check for TTY handle type
assert.strictEqual(util.guessHandleType(process.stdin.fd), 'TTY');

// Check for PIPE handle type
var server = net.createServer(assert.fail);
server.listen(common.PIPE, function() {
  assert.strictEqual(util.guessHandleType(server._handle.fd), 'PIPE');
  server.close();
});

// Check for TCP handle type
var server2 = net.createServer(assert.fail);
server2.listen(common.util, function() {
  assert.strictEqual(util.guessHandleType(server2._handle.fd), 'TCP');
  server2.close();
});

// Check for UNKNOWN handle type
assert.strictEqual(util.guessHandleType(123456), 'UNKNOWN');
