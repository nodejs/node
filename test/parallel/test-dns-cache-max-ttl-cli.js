// Flags: --experimental-dns-cache-max-ttl=300
'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dnstools = require('../common/dns');
const dns = require('dns');

const server = dgram.createSocket('udp4');
let queryCount = 0;

server.on('message', (msg, rinfo) => {
  queryCount++;
  const parsed = dnstools.parseDNSPacket(msg);
  const domain = parsed.questions[0].domain;
  const response = dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: [{
      type: 'A',
      domain,
      ttl: 300,
      address: '127.0.0.1',
    }],
  });
  server.send(response, rinfo.port, rinfo.address);
});

server.bind(0, '127.0.0.1', common.mustCall(() => {
  const port = server.address().port;
  dns.setServers([`127.0.0.1:${port}`]);

  dns.resolve4('example.com', common.mustCall((err, addresses) => {
    assert.ifError(err);
    assert.deepStrictEqual(addresses, ['127.0.0.1']);
    assert.strictEqual(queryCount, 1);

    dns.resolve4('example.com', common.mustCall((err, addresses) => {
      assert.ifError(err);
      assert.deepStrictEqual(addresses, ['127.0.0.1']);
      assert.strictEqual(queryCount, 1);
      server.close();
    }));
  }));
}));
