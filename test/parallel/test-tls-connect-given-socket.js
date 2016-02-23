'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var net = require('net');
var fs = require('fs');
var path = require('path');

var serverConnected = 0;
var clientConnected = 0;

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var server = tls.createServer(options, function(socket) {
  serverConnected++;
  socket.end('Hello');
}).listen(common.PORT, function() {
  var waiting = 2;
  function establish(socket) {
    var client = tls.connect({
      rejectUnauthorized: false,
      socket: socket
    }, function() {
      clientConnected++;
      var data = '';
      client.on('data', function(chunk) {
        data += chunk.toString();
      });
      client.on('end', function() {
        assert.equal(data, 'Hello');
        if (--waiting === 0)
          server.close();
      });
    });
    assert(client.readable);
    assert(client.writable);

    return client;
  }

  // Immediate death socket
  var immediateDeath = net.connect(common.PORT);
  establish(immediateDeath).destroy();

  // Outliving
  var outlivingTCP = net.connect(common.PORT);
  outlivingTCP.on('connect', function() {
    outlivingTLS.destroy();
    next();
  });
  var outlivingTLS = establish(outlivingTCP);

  function next() {
    // Already connected socket
    var connected = net.connect(common.PORT, function() {
      establish(connected);
    });

    // Connecting socket
    var connecting = net.connect(common.PORT);
    establish(connecting);

  }
});

process.on('exit', function() {
  assert.equal(serverConnected, 2);
  assert.equal(clientConnected, 2);
});
