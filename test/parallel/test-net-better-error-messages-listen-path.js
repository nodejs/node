'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
var fp = '/blah/fadfa';
var server = net.createServer(assert.fail);
server.listen(fp, assert.fail);
server.on('error', common.mustCall(function(e) {
  assert.equal(e.address, fp);
}));
