'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var fp = '/blah/fadfa';
var server = net.createServer(common.fail);
server.listen(fp, common.fail);
server.on('error', common.mustCall(function(e) {
  assert.equal(e.address, fp);
}));
