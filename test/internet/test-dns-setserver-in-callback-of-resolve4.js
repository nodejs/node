'use strict';

const common = require('../common');
const dns = require('dns');

dns.resolve4('google.com', common.mustCall(function(/* err, nameServers */) {
  // do not care about `err` and `nameServers`,
  // both failed and succeeded would be OK.
  //
  // just to test crash

  dns.setServers([ '8.8.8.8' ]);

  // the test case shouldn't crash here
  // see https://github.com/nodejs/node/pull/13050 and
  // https://github.com/nodejs/node/issues/894
}));
