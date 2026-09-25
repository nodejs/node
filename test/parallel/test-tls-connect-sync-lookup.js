'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('node:tls');

// Verify that a synchronous lookup cannot interrupt TLS socket initialization.
const controller = new AbortController();
const socket = tls.connect({
  host: 'example.com',
  servername: 'example.com',
  port: 443,
  signal: controller.signal,
  lookup(_hostname, _options, callback) {
    callback(null, [{ address: '2001:db8::1', family: 6 }]);
    controller.abort();
  },
});

socket.on('error', common.mustCall());
