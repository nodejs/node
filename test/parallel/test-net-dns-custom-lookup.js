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
        if (dnsopts.all) {
          cb(null, [{ address: common.localhostIPv4, family: 4 }]);
        } else {
          cb(null, common.localhostIPv4, 4);
        }
      });
    } else {
      process.nextTick(function() {
        if (dnsopts.all) {
          cb(null, [{ address: '::1', family: 6 }]);
        } else {
          cb(null, '::1', 6);
        }
      });
    }
  }
}

check(4, function() {
  common.hasIPv6 && check(6);
});

// Verify that bad lookup() IPs are handled.
{
  net.connect({
    host: 'localhost',
    port: 80,
    lookup(host, dnsopts, cb) {
      if (dnsopts.all) {
        cb(null, [{ address: undefined, family: 4 }]);
      } else {
        cb(null, undefined, 4);
      }
    }
  }).on('error', common.expectsError({ code: 'ERR_INVALID_IP_ADDRESS' }));
}
