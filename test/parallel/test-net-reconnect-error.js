'use strict';
var common = require('../common');
var net = require('net');
var assert = require('assert');
var N = 20;
var client_error_count = 0;
var disconnect_count = 0;

// Hopefully nothing is running on common.PORT
var c = net.createConnection(common.PORT);

c.on('connect', function() {
  console.error('CLIENT connected');
  assert.ok(false);
});

c.on('error', function(e) {
  console.error('CLIENT error: ' + e.code);
  client_error_count++;
  assert.equal('ECONNREFUSED', e.code);
});

c.on('close', function() {
  console.log('CLIENT disconnect');
  if (disconnect_count++ < N)
    c.connect(common.PORT); // reconnect
});

process.on('exit', function() {
  assert.equal(N + 1, disconnect_count);
  assert.equal(N + 1, client_error_count);
});
