'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const fp = '/blah/fadfa';
const server = net.createServer(common.fail);
server.listen(fp, common.fail);
server.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.address, fp);
}));
