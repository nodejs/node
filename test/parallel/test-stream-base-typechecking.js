'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer().listen(0, common.mustCall(() => {
  const client = net.connect(server.address().port, common.mustCall(() => {
    assert.throws(() => {
      client.write('broken', 'buffer');
    }, /^TypeError: Second argument must be a buffer$/);
    client.destroy();
    server.close();
  }));
}));
