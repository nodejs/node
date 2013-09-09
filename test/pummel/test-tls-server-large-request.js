// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');
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
