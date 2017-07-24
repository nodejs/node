'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const dns = require('dns');
const assert = require('assert');
const dgram = require('dgram');

const server = dgram.createSocket('udp4');

server.on('message', common.mustCall((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const domain = parsed.questions[0].domain;
  assert.strictEqual(domain, 'example.org');

  const buf = dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: { type: 'A', address: '1.2.3.4', ttl: 123, domain },
  });
  // Overwrite the # of answers with 2, which is incorrect.
  buf.writeUInt16LE(2, 6);
  server.send(buf, port, address);
}));

server.bind(0, common.mustCall(() => {
  const address = server.address();
  dns.setServers([`127.0.0.1:${address.port}`]);

  dns.resolveAny('example.org', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADRESP');
    assert.strictEqual(err.syscall, 'queryAny');
    assert.strictEqual(err.hostname, 'example.org');
    server.close();
  }));
}));
