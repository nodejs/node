var common = require('../common');

if (!process.features.tls_ocsp) {
  console.error('Skipping because node compiled without OpenSSL or ' +
                'with old OpenSSL version.');
  process.exit(0);
}

var assert = require('assert');
var tls = require('tls');
var constants = require('constants');
var fs = require('fs');
var join = require('path').join;

var keyFile = join(common.fixturesDir, 'keys', 'agent1-key.pem');
var certFile = join(common.fixturesDir, 'keys', 'agent1-cert.pem');
var caFile = join(common.fixturesDir, 'keys', 'ca1-cert.pem');
var key = fs.readFileSync(keyFile);
var cert = fs.readFileSync(certFile);

var server = tls.createServer({
  cert: cert,
  key: key
}, function (socket) {
  socket.destroySoon();
});

server.on('resumeSession', common.mustCall(function() {
  // Should not be actually called
}, 0));

server.listen(common.PORT, function() {
  var client = tls.connect({
    rejectUnauthorized: false,
    port: common.PORT,

    // Just to make sure that `newSession` is going to be called
    secureOptions: constants.SSL_OP_NO_TICKET
  }, function() {
    server.close();
  });
});
