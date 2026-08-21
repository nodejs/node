'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const { Resolver } = require('dns');
const assert = require('assert');
const dgram = require('dgram');

const server = dgram.createSocket('udp4');
const resolver = new Resolver();

server.bind(0, common.mustCall(() => {
  resolver.setServers([`127.0.0.1:${server.address().port}`]);
  resolver.reverse('123.45.67.89', common.mustCall((err, res) => {
    assert.strictEqual(err.code, 'ECANCELLED');
    assert.strictEqual(err.syscall, 'getHostByAddr');
    assert.strictEqual(err.hostname, '123.45.67.89');
    server.close();
  }));
}));

server.on('message', common.mustCall((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const domain = parsed.questions[0].domain;
  assert.strictEqual(domain, '89.67.45.123.in-addr.arpa');

  // Do not send a reply.
  resolver.cancel();
}));
