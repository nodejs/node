'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');
const N = 20;
let client_error_count = 0;
let disconnect_count = 0;

const c = net.createConnection(common.PORT);

c.on('connect', function() {
  console.error('CLIENT connected');
  assert.ok(false);
});

c.on('error', common.mustCall((e) => {
  client_error_count++;
  assert.strictEqual('ECONNREFUSED', e.code);
}, N + 1));

c.on('close', common.mustCall(() => {
  if (disconnect_count++ < N)
    c.connect(common.PORT); // reconnect
}, N + 1));

process.on('exit', function() {
  assert.strictEqual(N + 1, disconnect_count);
  assert.strictEqual(N + 1, client_error_count);
});
