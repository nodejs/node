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

'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

function pingPongTest(port, host) {
  const N = 1000;
  let count = 0;
  let sentPongs = 0;
  let sent_final_ping = false;

  const server = net.createServer(
    { allowHalfOpen: true },
    common.mustCall(onSocket)
  );

  function onSocket(socket) {
    assert.strictEqual(socket.server, server);
    assert.strictEqual(
      server,
      server.getConnections(common.mustCall(function(err, connections) {
        assert.ifError(err);
        assert.strictEqual(connections, 1);
      }))
    );

    socket.setNoDelay();
    socket.timeout = 0;

    socket.setEncoding('utf8');
    socket.on('data', common.mustCall(function(data) {
      // Since we never queue data (we're always waiting for the PING
      // before sending a pong) the writeQueueSize should always be less
      // than one message.
      assert.ok(socket.bufferSize >= 0 && socket.bufferSize <= 4);

      assert.strictEqual(socket.writable, true);
      assert.strictEqual(socket.readable, true);
      assert.ok(count <= N);
      assert.strictEqual(data, 'PING');

      socket.write('PONG', common.mustCall(function() {
        sentPongs++;
      }));
    }, N + 1));

    socket.on('end', common.mustCall(function() {
      assert.strictEqual(socket.allowHalfOpen, true);
      assert.strictEqual(socket.writable, true); // because allowHalfOpen
      assert.strictEqual(socket.readable, false);
      socket.end();
    }));

    socket.on('error', common.mustNotCall());

    socket.on('close', common.mustCall(function() {
      assert.strictEqual(socket.writable, false);
      assert.strictEqual(socket.readable, false);
      socket.server.close();
    }));
  }


  server.listen(port, host, common.mustCall(function() {
    if (this.address().port)
      port = this.address().port;

    const client = net.createConnection(port, host);

    client.setEncoding('ascii');
    client.on('connect', common.mustCall(function() {
      assert.strictEqual(client.readable, true);
      assert.strictEqual(client.writable, true);
      client.write('PING');
    }));

    client.on('data', common.mustCall(function(data) {
      assert.strictEqual(data, 'PONG');
      count += 1;

      if (sent_final_ping) {
        assert.strictEqual(client.writable, false);
        assert.strictEqual(client.readable, true);
        return;
      } else {
        assert.strictEqual(client.writable, true);
        assert.strictEqual(client.readable, true);
      }

      if (count < N) {
        client.write('PING');
      } else {
        sent_final_ping = true;
        client.write('PING');
        client.end();
      }
    }, N + 1));

    client.on('close', common.mustCall(function() {
      assert.strictEqual(count, N + 1);
      assert.strictEqual(sentPongs, N + 1);
      assert.strictEqual(sent_final_ping, true);
    }));

    client.on('error', common.mustNotCall());
  }));
}

/* All are run at once, so run on different ports */
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
pingPongTest(common.PIPE);
pingPongTest(0);
pingPongTest(0, 'localhost');
if (common.hasIPv6)
  pingPongTest(0, '::1');
