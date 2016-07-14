'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');

var out = '';

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  c.end('hello');
}).listen(0, function() {
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

  socket.connect(this.address().port);
});

process.on('exit', function() {
  assert.equal(out, 'hello');
});
