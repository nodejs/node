'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const dns = require('dns');
const { mock } = require('node:test');

if (!common.hasIPv6) {
  common.printSkipMessage('ipv6 part of test, no IPv6 support');
  return;
}

// Test on IPv6 Server, dns.lookup throws an error
{
  mock.method(dns, 'lookup', (hostname, options, callback) => {
    callback(new Error('Mocked error'));
  });
  const host = 'ipv6_loopback';

  const server = net.createServer();

  server.on('error', common.mustCall((e) => {
    assert.strictEqual(e.message, 'Mocked error');
  }));

  server.listen(common.PORT + 2, host);
}


// Test on IPv6 Server, server.listen throws an error
{
  mock.method(dns, 'lookup', (hostname, options, callback) => {
    if (hostname === 'ipv6_loopback') {
      callback(null, [{ address: 'fe80::1', family: 6 }]);
    } else {
      dns.lookup.wrappedMethod(hostname, options, callback);
    }
  });
  const host = 'ipv6_loopback';

  const server = net.createServer();

  server.on('error', common.mustCall((e) => {
    assert.strictEqual(e.address, 'fe80::1');
    assert.strictEqual(e.syscall, 'listen');
    assert.strictEqual(e.errno, -49);
  }));

  server.listen(common.PORT + 2, host);
}

// Test on IPv6 Server, picks 127.0.0.1 between that and fe80::1
{

  mock.method(dns, 'lookup', (hostname, options, callback) => {
    if (hostname === 'ipv6_loopback_with_double_entry') {
      callback(null, [{ address: 'fe80::1', family: 6 },
                      { address: '127.0.0.1', family: 4 }]);
    } else {
      dns.lookup.wrappedMethod(hostname, options, callback);
    }
  });

  const host = 'ipv6_loopback_with_double_entry';
  const family4 = 'IPv4';

  const server = net.createServer();

  server.on('error', common.mustNotCall());

  server.listen(common.PORT + 3, host, common.mustCall(() => {
    const address = server.address();
    assert.strictEqual(address.address, '127.0.0.1');
    assert.strictEqual(address.port, common.PORT + 3);
    assert.strictEqual(address.family, family4);
    server.close();
  }));
}
