var common = require('../common');
var assert = require('assert');
var net = require('net');
var dns = require('dns');
var ok = false;

function check(addressType, cb) {
  var server = net.createServer(function(client) {
    client.end();
    server.close();
    cb && cb();
  });

  var address = addressType === 4 ? '127.0.0.1' : '::1';
  server.listen(common.PORT, address, function() {
    net.connect({
      port: common.PORT,
      host: 'localhost',
      lookup: lookup
    }).on('lookup', function(err, ip, type) {
      assert.equal(err, null);
      assert.equal(ip, address);
      assert.equal(type, addressType);
      ok = true;
    });
  });

  function lookup(host, dnsopts, cb) {
    dnsopts.family = addressType;
    dns.lookup(host, dnsopts, cb);
  }
}

check(4, function() {
  common.hasIPv6 && check(6);
});

process.on('exit', function() {
  assert.ok(ok);
});
