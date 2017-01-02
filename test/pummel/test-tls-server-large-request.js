'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var stream = require('stream');
var util = require('util');

var request = Buffer.from(new Array(1024 * 256).join('ABCD')); // 1mb

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

function Mediator() {
  stream.Writable.call(this);
  this.buf = '';
}
util.inherits(Mediator, stream.Writable);

Mediator.prototype._write = function _write(data, enc, cb) {
  this.buf += data;
  setTimeout(cb, 0);

  if (this.buf.length >= request.length) {
    assert.equal(this.buf, request.toString());
    server.close();
  }
};

var mediator = new Mediator();

var server = tls.Server(options, common.mustCall(function(socket) {
  socket.pipe(mediator);
}));

server.listen(common.PORT, common.mustCall(function() {
  var client1 = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    client1.end(request);
  }));
}));
