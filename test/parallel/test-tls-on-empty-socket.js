if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var assert = require('assert');
var fs = require('fs');
var net = require('net');
var tls = require('tls');

var common = require('../common');

var out = '';

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  c.end('hello');
}).listen(common.PORT, function() {
  var socket = new net.Socket();

  var s = tls.connect({
    socket: socket,
    rejectUnauthorized: false
  }, function() {
    s.on('data', function(chunk) {
      out += chunk;
    });
    s.on('end', function() {
      s.destroy();
      server.close();
    });
  });

  socket.connect(common.PORT);
});

process.on('exit', function() {
  assert.equal(out, 'hello');
});
