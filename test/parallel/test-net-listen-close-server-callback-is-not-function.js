'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(assert.fail);
var closeEvents = 0;

server.on('close', function() {
  ++closeEvents;
});

server.listen(0, function() {
  assert(false);
});

server.close('bad argument');

process.on('exit', function() {
  assert.equal(closeEvents, 1);
});
