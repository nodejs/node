'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');
const N = 20;
let client_error_count = 0;
let disconnect_count = 0;

// Hopefully nothing is running on common.PORT
const c = net.createConnection(common.PORT);

c.on('connect', common.mustNotCall('client should not have connected'));

c.on('error', function(e) {
  console.error('CLIENT error: ' + e.code);
  client_error_count++;
  assert.strictEqual('ECONNREFUSED', e.code);
});

c.on('close', function() {
  console.log('CLIENT disconnect');
  if (disconnect_count++ < N)
    c.connect(common.PORT); // reconnect
});

process.on('exit', function() {
  assert.strictEqual(N + 1, disconnect_count);
  assert.strictEqual(N + 1, client_error_count);
});
