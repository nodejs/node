'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');

const server = dgram.createSocket('udp4');

server.on('message', common.mustCall((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const domain = parsed.questions[0].domain;
  assert.strictEqual(domain, 'example.org');

  server.send(dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: [{ type: 'A', address: '1.2.3.4', ttl: 123, domain }],
    additionalRecords: [
      { type: 'A', address: '1.2.3.4', ttl: 123, domain: 'x y.example.org' },
    ],
  }), port, address);
}));

server.bind(0, common.mustCall(() => {
  const resolver = new dns.Resolver({ timeout: 1000, tries: 1 });
  resolver.setServers([`127.0.0.1:${server.address().port}`]);

  resolver.resolve4('example.org', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADNAME');
    assert.strictEqual(err.syscall, 'queryA');
    assert.strictEqual(err.hostname, 'example.org');
    server.close();
  }));
}));
