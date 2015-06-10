'use strict';

const common = require('../common');
const net = require('net');
const assert = require('assert');
const fs = require('fs');

if (process.platform === 'win32') {
  console.log('1..0 # Skipped: no RLIMIT_NOFILE on Windows');
  return;
}

// Exhaust the file descriptors
try {
  while (true) {
    fs.openSync(__filename, 'r');
  }
} catch (err) {
  assert(err.code === 'EMFILE' || err.code === 'ENFILE');
}

// Now try to connect. If the `host` is omitted, by default, `localhost` will
// be used. To actually connect, hostname to IP translation has to happen. So,
// when `dns` module does lookup, `getaddrinfo` should throw `EMFILE` error.
//
// NOTE: We actually don't need a server, because fds are already exhausted.
// So, `connect` calls will fail.
net.connect({
  port: common.PORT
}).on('error', common.mustCall(function(er) {
  assert.equal(er.message, 'getaddrinfo EMFILE');
  assert.equal(er.code, 'EMFILE');
  assert.equal(er.syscall, 'getaddrinfo');
  assert.equal(er.port, common.PORT);
}));

// Again connect to the server with the host option, `127.0.0.1`. We should get
// `EMFILE` error from `connect` now.
net.connect({
  host: common.localhostIPv4,
  port: common.PORT
}).on('error', common.mustCall(function(er) {
  assert.equal(er.message,
               `connect EMFILE ${common.localhostIPv4}:${common.PORT}`);
  assert.equal(er.code, 'EMFILE');
  assert.equal(er.syscall, 'connect');
  assert.equal(er.port, common.PORT);
}));
