'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const { Resolver } = require('dns');
const assert = require('assert');
const dgram = require('dgram');

const server = dgram.createSocket('udp4');
const resolver = new Resolver();

const expectedDomains = [];
const receivedDomains = [];
const desiredQueries = 11;
let finishedQueries = 0;

server.bind(0, common.mustCall(async () => {
  resolver.setServers([`127.0.0.1:${server.address().port}`]);

  const callback = common.mustCall((err, res) => {
    assert.strictEqual(err.code, 'ECANCELLED');
    assert.strictEqual(err.syscall, 'queryA');
    assert.strictEqual(err.hostname, `example${finishedQueries}.org`);

    finishedQueries++;
    if (finishedQueries === desiredQueries) {
      assert.deepStrictEqual(expectedDomains.sort(), receivedDomains.sort());
      server.close();
    }
  }, desiredQueries);

  const next = (...args) => {
    callback(...args);

    // Multiple queries
    for (let i = 1; i < desiredQueries; i++) {
      const domain = `example${i}.org`;
      expectedDomains.push(domain);
      resolver.resolve4(domain, callback);
    }
  };

  // Single query
  expectedDomains.push('example0.org');
  resolver.resolve4('example0.org', next);
}));

server.on('message', (msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);

  for (const question of parsed.questions) {
    receivedDomains.push(question.domain);
  }

  // Do not send a reply.
  resolver.cancel();
});
