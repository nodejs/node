'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

// Using port 0 as hostname used is already invalid.
const c = net.createConnection(0, 'this.hostname.is.invalid');

c.on('connect', common.mustNotCall());

c.on('error', common.mustCall(function(e) {
  // If Name Service Switch is available on the operating system then it
  // might be configured differently (/etc/nsswitch.conf).
  // If the system is configured with no dns the error code will be EAI_AGAIN,
  // but if there are more services after the dns entry, for example some
  // linux distributions ship a myhostname service by default which would
  // still produce the ENOTFOUND error.
  assert.ok(e.code === 'ENOTFOUND' || e.code === 'EAI_AGAIN');
  assert.strictEqual(e.port, 0);
  assert.strictEqual(e.hostname, 'this.hostname.is.invalid');
}));
