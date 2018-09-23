'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var client, killed = false, ended = false;
var TIMEOUT = 10 * 1000;

client = net.createConnection(53, '8.8.8.8', function() {
  client.unref();
});

client.on('close', function() {
  ended = true;
});

setTimeout(function() {
  killed = true;
  client.end();
}, TIMEOUT).unref();

process.on('exit', function() {
  assert.strictEqual(killed, false, 'A client should have connected');
  assert.strictEqual(ended, false, 'A client should stay connected');
});
