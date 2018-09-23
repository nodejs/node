'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var closed = false;
var server = net.createServer();

// unref before listening
server.unref();
server.listen();

// If the timeout fires, that means the server held the event loop open
// and the unref() was not persistent. Close the server and fail the test.
setTimeout(function() {
  closed = true;
  server.close();
}, 1000).unref();

process.on('exit', function() {
  assert.strictEqual(closed, false, 'server should not hold loop open');
});
