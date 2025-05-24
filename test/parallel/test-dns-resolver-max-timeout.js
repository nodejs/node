'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const dns = require('dns');
const assert = require('assert');
const dgram = require('dgram');

const server = dgram.createSocket('udp4');
const nxdomain = 'nxdomain.org';
const domain = 'example.org';
const answers = [{ type: 'A', address: '1.2.3.4', ttl: 123, domain }];

server.on('message', common.mustCallAtLeast((msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);
  if (parsed.questions[0].domain === nxdomain) {
    return;
  }
  assert.strictEqual(parsed.questions[0].domain, domain);
  server.send(dnstools.writeDNSPacket({
    id: parsed.id,
    questions: parsed.questions,
    answers: answers,
  }), port, address);
}), 1);

server.bind(0, common.mustCall(async () => {
  const address = server.address();
  // Test if the Resolver works as before.
  const resolver = new dns.promises.Resolver({ timeout: 1000, tries: 1, maxTimeouts: 1000 });
  resolver.setServers([`127.0.0.1:${address.port}`]);
  const res = await resolver.resolveAny('example.org');
  assert.strictEqual(res.length, 1);
  assert.strictEqual(res.length, answers.length);
  assert.strictEqual(res[0].address, answers[0].address);

  // Test that maxTimeout is effective.
  // Without maxTimeout, the timeout will keep increasing when retrying.
  const timeout1 = await timeout(address, { timeout: 500, tries: 3 });
  // With maxTimeout, the timeout will always 500 when retrying.
  const timeout2 = await timeout(address, { timeout: 500, tries: 3, maxTimeout: 500 });
  console.log(`timeout1: ${timeout1}, timeout2: ${timeout2}`);
  assert.strictEqual(timeout1 !== undefined && timeout2 !== undefined, true);
  assert.strictEqual(timeout1 > timeout2, true);
  server.close();
}));

async function timeout(address, options) {
  const start = Date.now();
  const resolver = new dns.promises.Resolver(options);
  resolver.setServers([`127.0.0.1:${address.port}`]);
  try {
    await resolver.resolveAny(nxdomain);
  } catch (e) {
    assert.strictEqual(e.code, 'ETIMEOUT');
    return Date.now() - start;
  }
}
