'use strict';

// We don't care about `err` in the callback function of `dns.resolve4`. We just
// want to test whether `dns.setServers` that is run after `resolve4` will cause
// a crash or not. If it doesn't crash, the test succeeded.

const common = require('../common');
const { addresses } = require('../common/internet');
const dns = require('dns');

dns.resolve4(
  addresses.INET4_HOST,
  common.mustCall(function(/* err, nameServers */) {
    dns.setServers([ addresses.DNS4_SERVER ]);
  }));

// Test https://github.com/nodejs/node/issues/14734
dns.resolve4(addresses.INET4_HOST, common.mustCall());
