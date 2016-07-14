'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(common.fail);
var closeEvents = 0;

server.on('close', function() {
  ++closeEvents;
});

server.listen(0, common.fail);

server.close('bad argument');

process.on('exit', function() {
  assert.equal(closeEvents, 1);
});
