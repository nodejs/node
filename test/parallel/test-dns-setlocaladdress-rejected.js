'use strict';
const common = require('../common');

// Verifies that a call to resolver.setLocalAddress() that throws leaves the
// previously configured local address in place.

// Binding to 127.0.0.0/8 addresses other than 127.0.0.1 works out of the box
// on Linux only.
if (!common.isLinux)
  common.skip('requires the whole 127.0.0.0/8 range on the loopback interface');

const assert = require('assert');
const dgram = require('dgram');
const { Resolver } = require('dns');

const resolver = new Resolver({ timeout: 1000, tries: 1 });
const server = dgram.createSocket('udp4');

server.on('message', common.mustCall((msg, rinfo) => {
  assert.strictEqual(rinfo.address, '127.0.0.2');
  server.close();
  resolver.cancel();
}));

server.bind(0, '127.0.0.1', common.mustCall(() => {
  resolver.setServers([`127.0.0.1:${server.address().port}`]);
  resolver.setLocalAddress('127.0.0.2');

  assert.throws(() => {
    resolver.setLocalAddress('127.0.0.3', '127.0.0.4');
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    message: 'Cannot specify two IPv4 addresses.',
  });
  assert.throws(() => {
    resolver.setLocalAddress('127.0.0.5', 'bad');
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    message: 'Invalid IP address.',
  });

  resolver.resolve4('example.test', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ECANCELLED');
  }));
}));
