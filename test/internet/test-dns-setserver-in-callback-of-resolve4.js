'use strict';

// Refs: https://github.com/nodejs/node/pull/13050
// We don't care about `err` in the callback function of `dns.resolve4` here.
// We just want to test whether `dns.setServers` here in the callback of
// `resolve4` will crash or not. If no crashing here, the test is succeeded.

const common = require('../common');
const dns = require('dns');

dns.resolve4('google.com', common.mustCall(function(/* err, nameServers */) {
  dns.setServers([ '8.8.8.8' ]);
}));
