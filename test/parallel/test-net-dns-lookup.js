'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(function(client) {
  client.end();
  server.close();
});

server.listen(0, '127.0.0.1', common.mustCall(function() {
  net.connect(this.address().port, 'localhost')
    .on('lookup', common.mustCall(function(err, ip, type) {
      assert.equal(err, null);
      assert.equal(ip, '127.0.0.1');
      assert.equal(type, '4');
    }));
}));
