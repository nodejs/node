'use strict';
// Regression test for SRV record resolution returning ECONNREFUSED.
//
// This test verifies that dns.resolveSrv() properly handles SRV queries
// and doesn't incorrectly return ECONNREFUSED errors when DNS servers
// are reachable but the query format or handling has issues.
//
// Background: In certain Node.js versions, SRV queries could fail with
// ECONNREFUSED even when the DNS server was accessible, affecting
// applications using MongoDB Atlas (mongodb+srv://) and other services
// that rely on SRV record discovery.

const common = require('../common');
const dnstools = require('../common/dns');
const dns = require('dns');
const dnsPromises = dns.promises;
const assert = require('assert');
const dgram = require('dgram');

// Test 1: Basic SRV resolution should succeed, not return ECONNREFUSED
{
  const server = dgram.createSocket('udp4');
  const srvRecord = {
    type: 'SRV',
    name: 'mongodb-server.cluster0.example.net',
    port: 27017,
    priority: 0,
    weight: 1,
    ttl: 60,
  };

  server.on('message', common.mustCall((msg, { address, port }) => {
    const parsed = dnstools.parseDNSPacket(msg);
    const domain = parsed.questions[0].domain;

    server.send(dnstools.writeDNSPacket({
      id: parsed.id,
      questions: parsed.questions,
      answers: [Object.assign({ domain }, srvRecord)],
    }), port, address);
  }));

  server.bind(0, common.mustCall(async () => {
    const { port } = server.address();
    const resolver = new dnsPromises.Resolver();
    resolver.setServers([`127.0.0.1:${port}`]);

    try {
      const result = await resolver.resolveSrv(
        '_mongodb._tcp.cluster0.example.net'
      );

      // Should NOT throw ECONNREFUSED
      assert.ok(Array.isArray(result));
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].name, 'mongodb-server.cluster0.example.net');
      assert.strictEqual(result[0].port, 27017);
      assert.strictEqual(result[0].priority, 0);
      assert.strictEqual(result[0].weight, 1);
    } catch (err) {
      // This is the regression: should NOT get ECONNREFUSED
      assert.notStrictEqual(err.code, 'ECONNREFUSED');
      throw err;
    } finally {
      server.close();
    }
  }));
}

// Test 2: Multiple SRV records (common for MongoDB Atlas clusters)
{
  const server = dgram.createSocket('udp4');
  const srvRecords = [
    { type: 'SRV', name: 'shard-00-00.cluster.mongodb.net', port: 27017, priority: 0, weight: 1, ttl: 60 },
    { type: 'SRV', name: 'shard-00-01.cluster.mongodb.net', port: 27017, priority: 0, weight: 1, ttl: 60 },
    { type: 'SRV', name: 'shard-00-02.cluster.mongodb.net', port: 27017, priority: 0, weight: 1, ttl: 60 },
  ];

  server.on('message', common.mustCall((msg, { address, port }) => {
    const parsed = dnstools.parseDNSPacket(msg);
    const domain = parsed.questions[0].domain;

    server.send(dnstools.writeDNSPacket({
      id: parsed.id,
      questions: parsed.questions,
      answers: srvRecords.map((r) => Object.assign({ domain }, r)),
    }), port, address);
  }));

  server.bind(0, common.mustCall(async () => {
    const { port } = server.address();
    const resolver = new dnsPromises.Resolver();
    resolver.setServers([`127.0.0.1:${port}`]);

    const result = await resolver.resolveSrv('_mongodb._tcp.cluster.mongodb.net');

    assert.strictEqual(result.length, 3);

    const names = result.map((r) => r.name).sort();
    assert.deepStrictEqual(names, [
      'shard-00-00.cluster.mongodb.net',
      'shard-00-01.cluster.mongodb.net',
      'shard-00-02.cluster.mongodb.net',
    ]);

    server.close();
  }));
}

// Test 3: Callback-based API should also work
{
  const server = dgram.createSocket('udp4');

  server.on('message', common.mustCall((msg, { address, port }) => {
    const parsed = dnstools.parseDNSPacket(msg);
    const domain = parsed.questions[0].domain;

    server.send(dnstools.writeDNSPacket({
      id: parsed.id,
      questions: parsed.questions,
      answers: [{
        domain,
        type: 'SRV',
        name: 'service.example.com',
        port: 443,
        priority: 10,
        weight: 5,
        ttl: 120,
      }],
    }), port, address);
  }));

  server.bind(0, common.mustCall(() => {
    const { port } = server.address();
    const resolver = new dns.Resolver();
    resolver.setServers([`127.0.0.1:${port}`]);

    resolver.resolveSrv('_https._tcp.example.com', common.mustSucceed((result) => {
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].name, 'service.example.com');
      assert.strictEqual(result[0].port, 443);
      assert.strictEqual(result[0].priority, 10);
      assert.strictEqual(result[0].weight, 5);
      server.close();
    }));
  }));
}
