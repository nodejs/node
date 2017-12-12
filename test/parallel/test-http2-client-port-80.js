'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const net = require('net');

// Verifies that port 80 gets set as expected

const connect = net.connect;
net.connect = common.mustCall((...args) => {
  assert.strictEqual(args[0], '80');
  return connect(...args);
});

const client = http2.connect('http://localhost:80');

// A socket error may or may not occur depending on whether there is something
// currently listening on port 80. Keep this as a non-op and not a mustCall or
// mustNotCall.
client.on('error', () => {});

client.close();
