'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const client = net.connect({
  host: 'this.hostname.is.invalid',
  port: common.PORT
});

client.once('error', common.mustCall((err) => {
  assert(err);
  assert.strictEqual(err.code, err.errno);
  assert.strictEqual(err.code, 'ENOTFOUND');
  assert.strictEqual(err.host, err.hostname);
  assert.strictEqual(err.host, 'this.hostname.is.invalid');
  assert.strictEqual(err.syscall, 'getaddrinfo');
}));

client.end();
