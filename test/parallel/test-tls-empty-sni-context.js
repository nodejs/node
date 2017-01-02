'use strict';

const common = require('../common');

if (!process.features.tls_sni) {
  console.log('1..0 # Skipped: node compiled without OpenSSL or ' +
              'with old OpenSSL version.');
  return;
}

const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

const tls = require('tls');

const options = {
  SNICallback: (name, callback) => {
    callback(null, tls.createSecureContext());
  }
};

const server = tls.createServer(options, (c) => {
  common.fail('Should not be called');
}).on('tlsClientError', common.mustCall((err, c) => {
  assert(/SSL_use_certificate:passed a null parameter/i.test(err.message));
  server.close();
})).listen(0, common.mustCall(() => {
  const c = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
    servername: 'any.name'
  }, () => {
    common.fail('Should not be called');
  });

  c.on('error', common.mustCall((err) => {
    assert(/socket hang up/.test(err.message));
  }));
}));
