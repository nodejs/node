'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  // ensure that the worker exits peacefully
  cluster.fork().on('exit', common.mustCall(function(statusCode) {
    assert.strictEqual(statusCode, 0);
  }));
} else {
  // listen() without port should not trigger a libuv assert
  net.createServer(common.mustNotCall()).listen(process.exit);
}
