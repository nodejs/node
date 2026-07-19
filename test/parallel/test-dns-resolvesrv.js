'use strict';
// Regression test for dns.resolveSrv() functionality.
// This test ensures SRV record resolution works correctly, which is
// critical for services like MongoDB Atlas that use SRV records for
// connection discovery (mongodb+srv:// URIs).
//
// Related issue: dns.resolveSrv() returning ECONNREFUSED instead of
// properly resolving SRV records.

const common = require('../common');
const dnstools = require('../common/dns');
const dns = require('dns');
const dnsPromises = dns.promises;
const assert = require('assert');
const dgram = require('dgram');

const srvRecords = [
  {
    type: 'SRV',
    name: 'server1.example.org',
    port: 27017,
    priority: 0,
    weight: 5,
    ttl: 300,
  },
  {
    type: 'SRV',
    name: 'server2.example.org',
    port: 27017,
    priority: 0,
    weight: 5,
    ttl: 300,
  },
  {
    type: 'SRV',
    name: 'server3.example.org',
    port: 27017,
    priority: 1,
    weight: 10,
    ttl: 300,
  },
];

const server = dgram.createSocket('udp4');

server.on('message', common.mustCall((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const domain = parsed.questions[0].domain;
  assert.strictEqual(domain, '_mongodb._tcp.cluster0.example.org');

  server.send(dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: srvRecords.map((record) => Object.assign({ domain }, record)),
  }), port, address);
}, 2));  // Called twice: once for callback, once for promises

server.bind(0, common.mustCall(async () => {
  const address = server.address();
  const resolver = new dns.Resolver();
  const resolverPromises = new dnsPromises.Resolver();

  resolver.setServers([`127.0.0.1:${address.port}`]);
  resolverPromises.setServers([`127.0.0.1:${address.port}`]);

  function validateResult(result) {
    assert.ok(Array.isArray(result), 'Result should be an array');
    assert.strictEqual(result.length, 3);

    for (const record of result) {
      assert.strictEqual(typeof record, 'object');
      assert.strictEqual(typeof record.name, 'string');
      assert.strictEqual(typeof record.port, 'number');
      assert.strictEqual(typeof record.priority, 'number');
      assert.strictEqual(typeof record.weight, 'number');
      assert.strictEqual(record.port, 27017);
    }

    // Verify we got all expected server names
    const names = result.map((r) => r.name).sort();
    assert.deepStrictEqual(names, [
      'server1.example.org',
      'server2.example.org',
      'server3.example.org',
    ]);
  }

  // Test promises API
  const promiseResult = await resolverPromises.resolveSrv(
    '_mongodb._tcp.cluster0.example.org'
  );
  validateResult(promiseResult);

  // Test callback API
  resolver.resolveSrv(
    '_mongodb._tcp.cluster0.example.org',
    common.mustSucceed((result) => {
      validateResult(result);
      server.close();
    })
  );
}));
