'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dnstools = require('../common/dns');
const dns = require('dns');

for (const ctor of [dns.Resolver, dns.promises.Resolver]) {
  for (const cacheMaxTTL of [null, true, false, '', '2', -1, 4.2]) {
    assert.throws(() => new ctor({ cacheMaxTTL }), {
      code: /ERR_INVALID_ARG_TYPE|ERR_OUT_OF_RANGE/,
    });
  }

  for (const cacheMaxTTL of [0, 1, 300, 3600]) {
    new ctor({ cacheMaxTTL });
  }
}

function createDNSServer(address, callback) {
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
        address,
      }],
    });
    server.send(response, rinfo.port, rinfo.address);
  });

  server.bind(0, '127.0.0.1', common.mustCall(() => {
    callback(server, () => queryCount);
  }));
}

createDNSServer('127.0.0.1', (server, getQueryCount) => {
  const port = server.address().port;
  const resolver = new dns.Resolver({ cacheMaxTTL: 300 });
  resolver.setServers([`127.0.0.1:${port}`]);

  resolver.resolve4('example.com', common.mustCall((err, addresses) => {
    assert.ifError(err);
    assert.deepStrictEqual(addresses, ['127.0.0.1']);
    assert.strictEqual(getQueryCount(), 1);

    resolver.resolve4('example.com', common.mustCall((err, addresses) => {
      assert.ifError(err);
      assert.deepStrictEqual(addresses, ['127.0.0.1']);
      assert.strictEqual(getQueryCount(), 1);
      server.close();
    }));
  }));
});

createDNSServer('127.0.0.2', (server, getQueryCount) => {
  const port = server.address().port;
  const resolver = new dns.Resolver({ cacheMaxTTL: 0 });
  resolver.setServers([`127.0.0.1:${port}`]);

  resolver.resolve4('example.com', common.mustCall((err, addresses) => {
    assert.ifError(err);
    assert.deepStrictEqual(addresses, ['127.0.0.2']);
    assert.strictEqual(getQueryCount(), 1);

    resolver.resolve4('example.com', common.mustCall((err, addresses) => {
      assert.ifError(err);
      assert.deepStrictEqual(addresses, ['127.0.0.2']);
      assert.strictEqual(getQueryCount(), 2);
      server.close();
    }));
  }));
});
