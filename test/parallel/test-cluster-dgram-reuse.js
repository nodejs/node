'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var dgram = require('dgram');

if (cluster.isMaster) {
  cluster.fork().on('exit', function(code) {
    assert.equal(code, 0);
  });
  return;
}

var sockets = [];
function next() {
  sockets.push(this);
  if (sockets.length !== 2)
    return;

  // Work around health check issue
  process.nextTick(function() {
    for (var i = 0; i < sockets.length; i++)
      sockets[i].close(close);
  });
}

var waiting = 2;
function close() {
  if (--waiting === 0)
    cluster.worker.disconnect();
}

for (var i = 0; i < 2; i++)
  dgram.createSocket({ type: 'udp4', reuseAddr: true }).bind(common.PORT, next);
