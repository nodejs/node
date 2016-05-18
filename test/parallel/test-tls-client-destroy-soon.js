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
var tls = require('tls');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

var big = Buffer.alloc(2 * 1024 * 1024, 'Y');
var connections = 0;
var bytesRead = 0;

// create server
var server = tls.createServer(options, function(socket) {
  socket.end(big);
  socket.destroySoon();
  connections++;
});

// start listening
server.listen(common.PORT, function() {
  var client = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, function() {
    client.on('readable', function() {
      var d = client.read();
      if (d)
        bytesRead += d.length;
    });

    client.on('end', function() {
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(1, connections);
  assert.equal(big.length, bytesRead);
});
