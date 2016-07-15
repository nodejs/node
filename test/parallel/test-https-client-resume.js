'use strict';
// Create an ssl server.  First connection, validate that not resume.
// Cache session and close connection.  Use session on second connection.
// ASSERT resumption.
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var tls = require('tls');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

// create server
var server = https.createServer(options, common.mustCall(function(req, res) {
  res.end('Goodbye');
}, 2));

// start listening
server.listen(0, function() {

  var session1 = null;
  var client1 = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, function() {
    console.log('connect1');
    assert.ok(!client1.isSessionReused(), 'Session *should not* be reused.');
    session1 = client1.getSession();
    client1.write('GET / HTTP/1.0\r\n' +
                  'Server: 127.0.0.1\r\n' +
                  '\r\n');
  });

  client1.on('close', function() {
    console.log('close1');

    var opts = {
      port: server.address().port,
      rejectUnauthorized: false,
      session: session1
    };

    var client2 = tls.connect(opts, function() {
      console.log('connect2');
      assert.ok(client2.isSessionReused(), 'Session *should* be reused.');
      client2.write('GET / HTTP/1.0\r\n' +
                    'Server: 127.0.0.1\r\n' +
                    '\r\n');
    });

    client2.on('close', function() {
      console.log('close2');
      server.close();
    });
  });
});
