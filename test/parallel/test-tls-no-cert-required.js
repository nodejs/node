var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

// Omitting the cert or pfx option to tls.createServer() should not throw.
// AECDH-NULL-SHA is a no-authentication/no-encryption cipher and hence
// doesn't need a certificate.
tls.createServer({ ciphers: 'AECDH-NULL-SHA' }).listen(common.PORT, function() {
  this.close();
});
