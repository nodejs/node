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
  // If Name Service Switch is available on the operating system then it
  // might be configured differently (/etc/nsswitch.conf).
  // If the system is configured with no dns the error code will be EAI_AGAIN,
  // but if there are more services after the dns entry, for example some
  // linux distributions ship a myhostname service by default which would
  // still produce the ENOTFOUND error.
  assert.ok(err.code === 'ENOTFOUND' || err.code === 'EAI_AGAIN');
  assert.strictEqual(err.host, err.hostname);
  assert.strictEqual(err.host, 'this.hostname.is.invalid');
  assert.strictEqual(err.syscall, 'getaddrinfo');
}));

client.end();
