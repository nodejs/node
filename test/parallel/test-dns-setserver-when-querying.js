'use strict';

const common = require('../common');
const assert = require('assert');

const dns = require('dns');

const goog = [
  '8.8.8.8',
  '8.8.4.4',
];

{
  // Fix https://github.com/nodejs/node/issues/14734
  const resolver = new dns.Resolver();
  resolver.resolve('localhost', function(err/*, ret*/) {
    // nothing
  });

  assert.throws(() => {
    resolver.setServers(goog);
  }, common.expectsError({
    code: 'ERR_DNS_SET_SERVERS_FAILED',
    message: /^c-ares failed to set servers: "There are pending queries\." \[.+\]$/g
  }));

  dns.resolve('localhost', function(err/*, ret*/) {
    // nothing
  });

  // should not throw
  dns.setServers(goog);
}
