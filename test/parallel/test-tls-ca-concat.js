'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// Check ca option can contain concatenated certs by prepending an unrelated
// non-CA cert and showing that agent6's CA root is still found.

const {
  connect, keys
} = require(fixtures.path('tls-connect'));

connect({
  client: {
    checkServerIdentity: (servername, cert) => { },
    ca: `${keys.agent1.cert}\n${keys.agent6.ca}`,
  },
  server: {
    cert: keys.agent6.cert,
    key: keys.agent6.key,
  },
}, common.mustSucceed((pair, cleanup) => {
  return cleanup();
}));
