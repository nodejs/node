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
var WriteWrap = process.binding('stream_wrap').WriteWrap;

var server = new TCP();

var r = server.bind('0.0.0.0', common.PORT);
assert.equal(0, r);

server.listen(128);

var slice, sliceCount = 0, eofCount = 0;

var writeCount = 0;
var recvCount = 0;

server.onconnection = function(err, client) {
  assert.equal(0, client.writeQueueSize);
  console.log('got connection');

  function maybeCloseClient() {
    if (client.pendingWrites.length == 0 && client.gotEOF) {
      console.log('close client');
      client.close();
    }
  }

  client.readStart();
  client.pendingWrites = [];
  client.onread = function(err, buffer) {
    if (buffer) {
      assert.ok(buffer.length > 0);

      assert.equal(0, client.writeQueueSize);

      var req = new WriteWrap();
      req.async = false;
      var err = client.writeBuffer(req, buffer);
      assert.equal(err, 0);
      client.pendingWrites.push(req);

      console.log('client.writeQueueSize: ' + client.writeQueueSize);
      // 11 bytes should flush
      assert.equal(0, client.writeQueueSize);

      if (req.async)
        req.oncomplete = done;
      else
        process.nextTick(done.bind(null, 0, client, req));

      function done(status, client_, req_) {
        assert.equal(req, client.pendingWrites.shift());

        // Check parameters.
        assert.equal(0, status);
        assert.equal(client, client_);
        assert.equal(req, req_);

        console.log('client.writeQueueSize: ' + client.writeQueueSize);
        assert.equal(0, client.writeQueueSize);

        writeCount++;
        console.log('write ' + writeCount);
        maybeCloseClient();
      };

      sliceCount++;
    } else {
      console.log('eof');
      client.gotEOF = true;
      server.close();
      eofCount++;
      maybeCloseClient();
    }
  };
};

var net = require('net');

var c = net.createConnection(common.PORT);
c.on('connect', function() {
  c.end('hello world');
});

c.setEncoding('utf8');
c.on('data', function(d) {
  assert.equal('hello world', d);
  recvCount++;
});

c.on('close', function() {
  console.error('client closed');
});

process.on('exit', function() {
  assert.equal(1, sliceCount);
  assert.equal(1, eofCount);
  assert.equal(1, writeCount);
  assert.equal(1, recvCount);
});
