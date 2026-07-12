'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');
const dnstools = require('../common/dns');

const server = dgram.createSocket('udp4');
const resolver = new dns.Resolver();
const expectedHostname = 'reverse-dns-only.example';

server.on('message', common.mustCall((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const question = parsed.questions[0];

  assert.strictEqual(question.domain, '1.0.0.127.in-addr.arpa');
  assert.strictEqual(question.type, 'PTR');

  server.send(dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: [{
      type: 'PTR',
      domain: question.domain,
      value: expectedHostname,
    }],
  }), port, address);
}));

server.bind(0, common.mustCall(() => {
  resolver.setServers([`127.0.0.1:${server.address().port}`]);
  resolver.reverse('127.0.0.1', common.mustSucceed((hostnames) => {
    assert.deepStrictEqual(hostnames, [expectedHostname]);
    server.close();
  }));
}));
