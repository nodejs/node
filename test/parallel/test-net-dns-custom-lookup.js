'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

function check(addressType, cb) {
  const server = net.createServer(function(client) {
    client.end();
    server.close();
    cb && cb();
  });

  const address = addressType === 4 ? common.localhostIPv4 : '::1';
  server.listen(0, address, common.mustCall(function() {
    net.connect({
      port: this.address().port,
      host: 'localhost',
      family: addressType,
      lookup: lookup
    }).on('lookup', common.mustCall(function(err, ip, type) {
      assert.strictEqual(err, null);
      assert.strictEqual(address, ip);
      assert.strictEqual(type, addressType);
    }));
  }));

  function lookup(host, dnsopts, cb) {
    dnsopts.family = addressType;
    if (addressType === 4) {
      process.nextTick(function() {
        cb(null, common.localhostIPv4, 4);
      });
    } else {
      process.nextTick(function() {
        cb(null, '::1', 6);
      });
    }
  }
}

check(4, function() {
  common.hasIPv6 && check(6);
});
