if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var assert = require('assert');
var fs = require('fs');
var net = require('net');
var tls = require('tls');

var common = require('../common');

var sent = 'hello world';
var received = '';
var ended = 0;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = net.createServer(function(c) {
  setTimeout(function() {
    var s = new tls.TLSSocket(c, {
      isServer: true,
      secureContext: tls.createSecureContext(options)
    });

    s.on('data', function(chunk) {
      received += chunk;
    });

    s.on('end', function() {
      ended++;
      server.close();
      s.destroy();
    });
  }, 200);
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
    c.end(sent);
  });
});

process.on('exit', function() {
  assert.equal(received, sent);
  assert.equal(ended, 1);
});
