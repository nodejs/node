'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');

const dnsPromises = dns.promises;

const kRecordCount = 257;
const kADomain = 'many-a.example.org';

const server = dgram.createSocket('udp4');

server.on('message', common.mustCall((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  const question = parsed.questions[0];
  const { domain } = question;

  assert.strictEqual(question.type, 'ANY');
  assert.strictEqual(domain, kADomain);

  server.send(dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: createARecords(domain),
  }), port, address);
}, 2));

server.bind(0, common.mustCall(async () => {
  const { port } = server.address();
  const callbackResolver = new dns.Resolver({ timeout: 1000, tries: 1 });
  const promiseResolver = new dnsPromises.Resolver({ timeout: 1000, tries: 1 });
  callbackResolver.setServers([`127.0.0.1:${port}`]);
  promiseResolver.setServers([`127.0.0.1:${port}`]);

  validateRecords(await promiseResolver.resolveAny(kADomain), 'A');
  validateRecords(await resolveAny(callbackResolver, kADomain), 'A');

  server.close();
}));

function createARecords(domain) {
  return Array.from({ length: kRecordCount }, (_, i) => ({
    type: 'A',
    address: `10.0.${i >> 8}.${i & 0xff}`,
    ttl: 60 + i,
    domain,
  }));
}

function resolveAny(resolver, domain) {
  return new Promise((resolve) => {
    resolver.resolveAny(domain, common.mustSucceed(resolve));
  });
}

function validateRecords(records, type) {
  assert.strictEqual(records.length, kRecordCount);
  for (const record of records) {
    assert.strictEqual(record.type, type);
  }

  assert.strictEqual(records[0].ttl, 60);
  assert.strictEqual(records[255].ttl, 315);
  assert.strictEqual(records[256].ttl, 316);
  assert.strictEqual(records[0].address, '10.0.0.0');
  assert.strictEqual(records[256].address, '10.0.1.0');
}
