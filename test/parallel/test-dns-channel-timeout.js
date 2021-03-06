'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');

for (const ctor of [dns.Resolver, dns.promises.Resolver]) {
  for (const timeout of [null, true, false, '', '2']) {
    assert.throws(() => new ctor({ timeout }), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
  }

  for (const timeout of [-2, 4.2, 2 ** 31]) {
    assert.throws(() => new ctor({ timeout }), {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    });
  }

  for (const timeout of [-1, 0, 1]) new ctor({ timeout });  // OK
}

for (const timeout of [0, 1, 2]) {
  const server = dgram.createSocket('udp4');
  server.bind(0, '127.0.0.1', common.mustCall(() => {
    const resolver = new dns.Resolver({ timeout });
    resolver.setServers([`127.0.0.1:${server.address().port}`]);
    resolver.resolve4('nodejs.org', common.mustCall((err) => {
      assert.throws(() => { throw err; }, {
        code: 'ETIMEOUT',
        name: 'Error',
      });
      server.close();
    }));
  }));
}

for (const timeout of [0, 1, 2]) {
  const server = dgram.createSocket('udp4');
  server.bind(0, '127.0.0.1', common.mustCall(() => {
    const resolver = new dns.promises.Resolver({ timeout });
    resolver.setServers([`127.0.0.1:${server.address().port}`]);
    resolver.resolve4('nodejs.org').catch(common.mustCall((err) => {
      assert.throws(() => { throw err; }, {
        code: 'ETIMEOUT',
        name: 'Error',
      });
      server.close();
    }));
  }));
}
