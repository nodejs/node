'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(function(client) {
  client.end();
  server.close();
});

server.listen(0, '127.0.0.1', common.mustCall(function() {
  net.connect(this.address().port, 'localhost')
    .on('lookup', common.mustCall(function(err, ip, type, host) {
      assert.strictEqual(err, null);
      assert.strictEqual(ip, '127.0.0.1');
      assert.strictEqual(type, 4);
      assert.strictEqual(host, 'localhost');
    }));
}));
