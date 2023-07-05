'use strict';

const common = require('../common');

// This test verifies that `tls.connect()` honors the `timeout` option when the
// socket is internally created.

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

const socket = tls.connect({
  port: 42,
  lookup: () => {},
  timeout: 1000
});

assert.strictEqual(socket.timeout, 1000);
