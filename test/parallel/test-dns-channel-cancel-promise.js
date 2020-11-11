'use strict';
const common = require('../common');
const { promises: dnsPromises } = require('dns');
const assert = require('assert');
const dgram = require('dgram');

const server = dgram.createSocket('udp4');
const resolver = new dnsPromises.Resolver();

const addMessageListener = () => {
  server.removeAllListeners('message');

  server.once('message', () => {
    server.once('message', common.mustNotCall);

    resolver.cancel();
  });
};

server.bind(0, common.mustCall(async () => {
  resolver.setServers([`127.0.0.1:${server.address().port}`]);

  addMessageListener();

  // Single promise
  {
    const hostname = 'example0.org';

    await assert.rejects(
      resolver.resolve4(hostname),
      {
        code: 'ECANCELLED',
        syscall: 'queryA',
        hostname
      }
    );
  }

  addMessageListener();

  // Multiple promises
  {
    const assertions = [];
    const assertionCount = 10;

    for (let i = 1; i <= assertionCount; i++) {
      const hostname = `example${i}.org`;

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

  server.close();
}));
