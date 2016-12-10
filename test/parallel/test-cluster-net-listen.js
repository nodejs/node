'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

if (cluster.isMaster) {
  // ensure that the worker exits peacefully
  cluster.fork().on('exit', common.mustCall(function(statusCode) {
    assert.equal(statusCode, 0);
  }));
} else {
  // listen() without port should not trigger a libuv assert
  net.createServer(common.fail).listen(process.exit);
}
