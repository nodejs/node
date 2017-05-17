'use strict';

// We don't care about `err` in the callback function of `dns.resolve4`. We just
// want to test whether `dns.setServers` that is run after `resolve4` will cause
// a crash or not. If it doesn't crash, the test succeeded.

const common = require('../common');
const dns = require('dns');

dns.resolve4('google.com', common.mustCall(function(/* err, nameServers */) {
  dns.setServers([ '8.8.8.8' ]);
}));
