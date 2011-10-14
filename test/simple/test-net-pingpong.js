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

var net = require('net');

var tests_run = 0;

function pingPongTest(port, host) {
  var N = 1000;
  var count = 0;
  var sentPongs = 0;
  var sent_final_ping = false;

  var server = net.createServer({ allowHalfOpen: true }, function(socket) {
    console.log('connection: ' + socket.remoteAddress);
    assert.equal(server, socket.server);
    assert.equal(1, server.connections);

    socket.setNoDelay();
    socket.timeout = 0;

    socket.setEncoding('utf8');
    socket.on('data', function(data) {
      // Since we never queue data (we're always waiting for the PING
      // before sending a pong) the writeQueueSize should always be less
      // than one message.
      assert.ok(0 <= socket.bufferSize && socket.bufferSize <= 4);

      console.log('server got: ' + data);
      assert.equal(true, socket.writable);
      assert.equal(true, socket.readable);
      assert.equal(true, count <= N);
      if (/PING/.exec(data)) {
        socket.write('PONG', function() {
          sentPongs++;
          console.error('sent PONG');
        });
      }
    });

    socket.on('end', function() {
      assert.equal(true, socket.writable); // because allowHalfOpen
      assert.equal(false, socket.readable);
      socket.end();
    });

    socket.on('error', function(e) {
      throw e;
    });

    socket.on('close', function() {
      console.log('server socket.endd');
      assert.equal(false, socket.writable);
      assert.equal(false, socket.readable);
      socket.server.close();
    });
  });


  server.listen(port, host, function() {
    console.log('server listening on ' + port + ' ' + host);

    var client = net.createConnection(port, host);

    client.setEncoding('ascii');
    client.on('connect', function() {
      assert.equal(true, client.readable);
      assert.equal(true, client.writable);
      client.write('PING');
    });

    client.on('data', function(data) {
      console.log('client got: ' + data);

      assert.equal('PONG', data);
      count += 1;

      if (sent_final_ping) {
        assert.equal(false, client.writable);
        assert.equal(true, client.readable);
        return;
      } else {
        assert.equal(true, client.writable);
        assert.equal(true, client.readable);
      }

      if (count < N) {
        client.write('PING');
      } else {
        sent_final_ping = true;
        client.write('PING');
        client.end();
      }
    });

    client.on('close', function() {
      console.log('client.end');
      assert.equal(N + 1, count);
      assert.equal(N + 1, sentPongs);
      assert.equal(true, sent_final_ping);
      tests_run += 1;
    });

    client.on('error', function(e) {
      throw e;
    });
  });
}

/* All are run at once, so run on different ports */
pingPongTest(common.PIPE);
pingPongTest(20988);
pingPongTest(20989, 'localhost');
pingPongTest(20997, '::1');

process.on('exit', function() {
  assert.equal(4, tests_run);
  console.log('done');
});
