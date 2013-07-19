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
var TCP = process.binding('tcp_wrap').TCP;

function makeConnection() {
  var client = new TCP();

  var req = {};
  var err = client.connect(req, '127.0.0.1', common.PORT);
  assert.equal(err, 0);

  req.oncomplete = function(status, client_, req_) {
    assert.equal(0, status);
    assert.equal(client, client_);
    assert.equal(req, req_);

    console.log('connected');
    var shutdownReq = {};
    var err = client.shutdown(shutdownReq);
    assert.equal(err, 0);

    shutdownReq.oncomplete = function(status, client_, req_) {
      console.log('shutdown complete');
      assert.equal(0, status);
      assert.equal(client, client_);
      assert.equal(shutdownReq, req_);
      shutdownCount++;
      client.close();
    };
  };
}

/////

var connectCount = 0;
var endCount = 0;
var shutdownCount = 0;

var server = require('net').Server(function(s) {
  console.log('got connection');
  connectCount++;
  s.resume();
  s.on('end', function() {
    console.log('got eof');
    endCount++;
    s.destroy();
    server.close();
  });
});

server.listen(common.PORT, makeConnection);

process.on('exit', function() {
  assert.equal(1, shutdownCount);
  assert.equal(1, connectCount);
  assert.equal(1, endCount);
});
