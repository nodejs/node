'use strict';
const common = require('../common');
// disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var tls = require('tls');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

// Force splitting incoming data
tls.SLAB_BUFFER_SIZE = 1;

var server = https.createServer(options);
server.on('upgrade', common.mustCall(function(req, socket, upgrade) {
  socket.on('data', function(data) {
    throw new Error('Unexpected data: ' + data);
  });
  socket.end('HTTP/1.1 200 Ok\r\n\r\n');
}));

server.listen(0, function() {
  var req = https.request({
    host: '127.0.0.1',
    port: this.address().port,
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
