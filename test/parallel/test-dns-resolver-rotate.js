'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const dns = require('dns');
const assert = require('assert');
const dgram = require('dgram');

function validate() {
  [
    -1,
    1.1,
    NaN,
    undefined,
    {},
    [],
    null,
    function() {},
    Symbol(),
    Infinity,
  ].forEach((rotate) => {
    try {
      new dns.Resolver({ rotate });
    } catch (e) {
      assert.ok(/ERR_INVALID_ARG_TYPE/i.test(e.code));
    }
  });
}

const domain = 'example.org';
const answers = [{ type: 'A', address: '1.2.3.4', ttl: 123, domain }];

function createServer() {
  return new Promise((resolve) => {
    const server = dgram.createSocket('udp4');
    server.on('message', (msg, { address, port }) => {
      const parsed = dnstools.parseDNSPacket(msg);
      server.send(dnstools.writeDNSPacket({
        id: parsed.id,
        questions: parsed.questions,
        answers: answers,
      }), port, address);
    });
    server.bind(0, common.mustCall(() => {
      resolve(server);
    }));
  });
}

function check(result, answers) {
  assert.strictEqual(result.length, answers.length);
  assert.strictEqual(result[0].type, answers[0].type);
  assert.strictEqual(result[0].address, answers[0].address);
  assert.strictEqual(result[0].ttl, answers[0].ttl);
}

async function main() {
  validate();
  {
    const resolver = new dns.promises.Resolver({ rotate: false });
    const server1 = await createServer();
    const server2 = await createServer();
    const address1 = server1.address();
    const address2 = server2.address();
    resolver.setServers([
      `127.0.0.1:${address1.port}`,
      `127.0.0.1:${address2.port}`,
    ]);
    server2.on('message', common.mustNotCall());
    const promises = [];
    // All queries should be sent to the server1
    for (let i = 0; i < 5; i++) {
      promises.push(resolver.resolveAny(domain));
    }
    const results = await Promise.all(promises);
    results.forEach((result) => check(result, answers));
    server1.close();
    server2.close();
  }

  {
    const resolver = new dns.promises.Resolver({ rotate: true });
    const serverPromises = [];
    for (let i = 0; i < 10; i++) {
      serverPromises.push(createServer());
    }
    const servers = await Promise.all(serverPromises);
    const addresses = [];
    let receiver = 0;
    servers.forEach((server) => {
      addresses.push(`127.0.0.1:${server.address().port}`);
      server.once('message', () => {
        receiver++;
      });
    });
    resolver.setServers(addresses);
    const queryPromises = [];
    // All queries should be randomly sent to the server (Unless the same server is chosen every time)
    for (let i = 0; i < 30; i++) {
      queryPromises.push(resolver.resolveAny(domain));
    }
    const results = await Promise.all(queryPromises);
    results.forEach((result) => check(result, answers));
    assert.ok(receiver > 1, `receiver: ${receiver}`);
    servers.forEach((server) => server.close());
  }
}

main();
