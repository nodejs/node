'use strict';
const common = require('../common');

// Verify connection with explicitly created client SecureContext.

const fixtures = require('../common/fixtures');
const {
  connect, keys, tls
} = require(fixtures.path('tls-connect'));

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
}, common.mustSucceed((pair, cleanup) => {
  return cleanup();
}));

connect({
  client: {
    servername: 'agent1',
    secureContext: tls.createSecureContext({
      ca: keys.agent1.ca,
      ciphers: null,
      clientCertEngine: null,
      crl: null,
      dhparam: null,
      passphrase: null,
      pfx: null,
      privateKeyIdentifier: null,
      privateKeyEngine: null,
      sessionIdContext: null,
      sessionTimeout: null,
      sigalgs: null,
      ticketKeys: null,
    }),
  },
  server: {
    cert: keys.agent1.cert,
    key: keys.agent1.key,
  },
}, common.mustSucceed((pair, cleanup) => {
  return cleanup();
}));
