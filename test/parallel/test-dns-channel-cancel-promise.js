'use strict';
const common = require('../common');
const dnstools = require('../common/dns');
const { promises: dnsPromises } = require('dns');
const assert = require('assert');
const dgram = require('dgram');

const server = dgram.createSocket('udp4');
const resolver = new dnsPromises.Resolver();

const receivedDomains = [];
const expectedDomains = [];

server.bind(0, common.mustCall(async () => {
  resolver.setServers([`127.0.0.1:${server.address().port}`]);

  // Single promise
  {
    const hostname = 'example0.org';
    expectedDomains.push(hostname);

    await assert.rejects(
      resolver.resolve4(hostname),
      {
        code: 'ECANCELLED',
        syscall: 'queryA',
        hostname: 'example0.org'
      }
    );
  }

  // Multiple promises
  {
    const assertions = [];
    const assertionCount = 10;

    for (let i = 1; i <= assertionCount; i++) {
      const hostname = `example${i}.org`;

      expectedDomains.push(hostname);

      assertions.push(
        assert.rejects(
          resolver.resolve4(hostname),
          {
            code: 'ECANCELLED',
            syscall: 'queryA',
            hostname: hostname
          }
        )
      );
    }

    await Promise.all(assertions);
  }

  assert.deepStrictEqual(expectedDomains.sort(), receivedDomains.sort());

  server.close();
}));

server.on('message', (msg, { address, port }) => {
  const parsed = dnstools.parseDNSPacket(msg);

  for (const question of parsed.questions) {
    receivedDomains.push(question.domain);
  }

  // Do not send a reply.
  resolver.cancel();
});
