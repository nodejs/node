'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

function check(addressType, cb) {
  var server = net.createServer(function(client) {
    client.end();
    server.close();
    cb && cb();
  });

  var address = addressType === 4 ? common.localhostIPv4 : '::1';
  server.listen(0, address, common.mustCall(function() {
    net.connect({
      port: this.address().port,
      host: 'localhost',
      family: addressType,
      lookup: lookup
    }).on('lookup', common.mustCall(function(err, ip, type) {
      assert.equal(err, null);
      assert.equal(address, ip);
      assert.equal(type, addressType);
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
