'use strict';
const common = require('../common');

// Verify connection with explicitly created client SecureContext.

const join = require('path').join;
const {
  assert, connect, keys, tls
} = require(join(common.fixturesDir, 'tls-connect'));

connect({
  client: {
    servername: 'agent1',
    secureContext: tls.createSecureContext({
      ca: keys.agent1.ca,
    }),
  },
  server: {
    cert: keys.agent1.cert,
    key: keys.agent1.key,
  },
}, function(err, pair, cleanup) {
  assert.ifError(err);
  return cleanup();
});
