'use strict';

const common = require('../common');

const dns = require('dns');

const goog = [
  '8.8.8.8',
  '8.8.4.4',
];

{
  // Fix https://github.com/nodejs/node/issues/14734

  {
    const resolver = new dns.Resolver();
    resolver.resolve('localhost', common.mustCall());

    common.expectsError(resolver.setServers.bind(resolver, goog), {
      code: 'ERR_DNS_SET_SERVERS_FAILED',
      message: /^c-ares failed to set servers: "There are pending queries\." \[.+\]$/g
    });
  }

  {
    dns.resolve('localhost', common.mustCall());

    // should not throw
    dns.setServers(goog);
  }
}
