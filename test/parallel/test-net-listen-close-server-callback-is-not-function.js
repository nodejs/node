'use strict';
const assert = require('assert');
const common = require('../common');
const net = require('net');

var server = net.createServer(assert.fail);
var closeEvents = 0;

server.on('close', function() {
  ++closeEvents;
});

server.listen(common.PORT, function() {
  assert(false);
});

server.close('bad argument');

process.on('exit', function() {
  assert.equal(closeEvents, 1);
});
