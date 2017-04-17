'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

const server = net.createServer();
server.listen(0);
const port = server.address().port;
const c = net.createConnection(port);

c.on('connect', common.mustNotCall());

c.on('error', common.mustCall((e) => {
  assert.strictEqual('ECONNREFUSED', e.code);
}));
server.close();
