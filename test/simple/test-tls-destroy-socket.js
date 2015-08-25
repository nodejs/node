if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var tls = require('tls');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

var normalSock = null;
var hasError = false;
var tlsCloseCount = 0;
var sockCloseCount = 0;

var server = tls.createServer(options, function(secureSock) {
  secureSock.on('error', function() {
    hasError = true;
  });
  secureSock.on('close', function() {
    tlsCloseCount++;
  });
  normalSock.on('close', function() {
    sockCloseCount++;
  });

  normalSock.destroy();
  secureSock.write('Test!', function(err) {
    assert(err);
  });
});

server.on('connection', function(sock) {
  normalSock = sock;
});

server.listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {rejectUnauthorized: false});
  c.on('error', function() {}); // ignore socket hangup error

  c.on('end', function() {
    server.close();
  });

  process.on('exit', function() {
    assert(hasError);
    assert.equal(tlsCloseCount, 1);
    assert.equal(sockCloseCount, 1);
  });
});
