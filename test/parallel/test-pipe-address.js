'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var server = net.createServer(common.fail);

common.refreshTmpDir();

server.listen(common.PIPE, common.mustCall(function() {
  assert.strictEqual(server.address(), common.PIPE);
  server.close();
}));
