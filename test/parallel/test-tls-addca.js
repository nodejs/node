'use strict';
const common = require('../common');

// Adding a CA certificate to contextWithCert should not also add it to
// contextWithoutCert. This is tested by trying to connect to a server that
// depends on that CA using contextWithoutCert.

const join = require('path').join;
const {
  assert, connect, keys, tls
} = require(join(common.fixturesDir, 'tls-connect'));

const contextWithoutCert = tls.createSecureContext({});
const contextWithCert = tls.createSecureContext({});
contextWithCert.context.addCACert(keys.agent1.ca);

const serverOptions = {
  key: keys.agent1.key,
  cert: keys.agent1.cert,
};

const clientOptions = {
  ca: [keys.agent1.ca],
  servername: 'agent1',
  rejectUnauthorized: true,
};

// This client should fail to connect because it doesn't trust the CA
// certificate.
clientOptions.secureContext = contextWithoutCert;

connect({
  client: clientOptions,
  server: serverOptions,
}, function(err, pair, cleanup) {
  assert(err);
  assert.strictEqual(err.message, 'unable to verify the first certificate');
  cleanup();

  // This time it should connect because contextWithCert includes the needed CA
  // certificate.
  clientOptions.secureContext = contextWithCert;
  connect({
    client: clientOptions,
    server: serverOptions,
  }, function(err, pair, cleanup) {
    assert.ifError(err);
    cleanup();
  });
});
