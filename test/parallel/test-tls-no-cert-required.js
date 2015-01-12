if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var tls = require('tls');

// Omitting the cert or pfx option to tls.createServer() should not throw.
// AECDH-NULL-SHA is a no-authentication/no-encryption cipher and hence
// doesn't need a certificate.
tls.createServer({ ciphers: 'AECDH-NULL-SHA' }).listen(common.PORT, function() {
  this.close();
});
