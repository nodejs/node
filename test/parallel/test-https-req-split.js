'use strict';
// disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var tls = require('tls');
var fs = require('fs');

var seen_req = false;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

// Force splitting incoming data
tls.SLAB_BUFFER_SIZE = 1;

var server = https.createServer(options);
server.on('upgrade', function(req, socket, upgrade) {
  socket.on('data', function(data) {
    throw new Error('Unexpected data: ' + data);
  });
  socket.end('HTTP/1.1 200 Ok\r\n\r\n');
  seen_req = true;
});

server.listen(common.PORT, function() {
  var req = https.request({
    host: '127.0.0.1',
    port: common.PORT,
    agent: false,
    headers: {
      Connection: 'Upgrade',
      Upgrade: 'Websocket'
    }
  }, function() {
    req.socket.destroy();
    server.close();
  });

  req.end();
});

process.on('exit', function() {
  assert(seen_req);
  console.log('ok');
});
