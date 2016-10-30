/*eslint max-len: ["error", { "ignoreComments": true }]*/

'use strict';

const common = require('../common');

if (!process.features.tls_sni) {
  common.skip('node compiled without OpenSSL or with old OpenSSL version.');
  return;
}

var isLibreSSL = /LibreSSL$/.test(process.versions.openssl);
if (isLibreSSL) {
  common.skip('Test not yet supported with LibreSSL');
  process.exit();
}

const assert = require('assert');

if (!common.hasCrypto) {
  return common.skip('missing crypto');
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
  //TODO: LibreSSL gives:
  // 140735295963904:error:1408A0C1:SSL routines:SSL3_GET_CLIENT_HELLO:no shared cipher:s3_srvr.c:1038:
  assert(/SSL_use_certificate:passed a null parameter/i.test(err.message));
  server.close();
})).listen(0, common.mustCall(() => {
  const c = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
    servername: 'any.name'
  }, common.mustNotCall());

  c.on('error', common.mustCall((err) => {
    //TODO: LibreSSL gives:
    // 140735295963904:error:14077410:SSL routines:SSL23_GET_SERVER_HELLO:sslv3 alert handshake failure:s23_clnt.c:441:
    assert(/socket hang up/.test(err.message));
  }));
}));
