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

function pingPongTest(port, host, on_complete) {
  var N = 100;
  var DELAY = 1;
  var count = 0;
  var client_ended = false;

  var server = net.createServer({ allowHalfOpen: true }, function(socket) {
    socket.setEncoding('utf8');

    socket.addListener('data', function(data) {
      console.log(data);
      assert.equal('PING', data);
      assert.equal('open', socket.readyState);
      assert.equal(true, count <= N);
      setTimeout(function() {
        assert.equal('open', socket.readyState);
        socket.write('PONG');
      }, DELAY);
    });

    socket.addListener('timeout', function() {
      common.debug('server-side timeout!!');
      assert.equal(false, true);
    });

    socket.addListener('end', function() {
      console.log('server-side socket EOF');
      assert.equal('writeOnly', socket.readyState);
      socket.end();
    });

    socket.addListener('close', function(had_error) {
      console.log('server-side socket.end');
      assert.equal(false, had_error);
      assert.equal('closed', socket.readyState);
      socket.server.close();
    });
  });

  server.listen(port, host, function() {
    var client = net.createConnection(port, host);

    client.setEncoding('utf8');

    client.addListener('connect', function() {
      assert.equal('open', client.readyState);
      client.write('PING');
    });

    client.addListener('data', function(data) {
      console.log(data);
      assert.equal('PONG', data);
      assert.equal('open', client.readyState);

      setTimeout(function() {
        assert.equal('open', client.readyState);
        if (count++ < N) {
          client.write('PING');
        } else {
          console.log('closing client');
          client.end();
          client_ended = true;
        }
      }, DELAY);
    });

    client.addListener('timeout', function() {
      common.debug('client-side timeout!!');
      assert.equal(false, true);
    });

    client.addListener('close', function() {
      console.log('client.end');
      assert.equal(N + 1, count);
      assert.ok(client_ended);
      if (on_complete) on_complete();
      tests_run += 1;
    });
  });
}

pingPongTest(common.PORT);

process.addListener('exit', function() {
  assert.equal(1, tests_run);
});
