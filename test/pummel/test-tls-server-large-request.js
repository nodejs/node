'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var stream = require('stream');
var util = require('util');

var clientConnected = 0;
var serverConnected = 0;
var request = new Buffer(new Array(1024 * 256).join('ABCD')); // 1mb

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

function Mediator() {
  stream.Writable.call(this);
  this.buf = '';
};
util.inherits(Mediator, stream.Writable);

Mediator.prototype._write = function write(data, enc, cb) {
  this.buf += data;
  setTimeout(cb, 0);

  if (this.buf.length >= request.length) {
    assert.equal(this.buf, request.toString());
    server.close();
  }
};

var mediator = new Mediator();

var server = tls.Server(options, function(socket) {
  socket.pipe(mediator);
  serverConnected++;
});

server.listen(common.PORT, function() {
  var client1 = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, function() {
    ++clientConnected;
    client1.end(request);
  });
});

process.on('exit', function() {
  assert.equal(clientConnected, 1);
  assert.equal(serverConnected, 1);
});
