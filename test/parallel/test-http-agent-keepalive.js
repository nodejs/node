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
const http = require('http');
const Agent = require('_http_agent').Agent;

let name;

const agent = new Agent({
  keepAlive: true,
  keepAliveMsecs: 1000,
  maxSockets: 5,
  maxFreeSockets: 5
});

const server = http.createServer(common.mustCall((req, res) => {
  if (req.url === '/error') {
    res.destroy();
    return;
  } else if (req.url === '/remote_close') {
    // Cache the socket, close it after a short delay
    const socket = res.connection;
    setImmediate(common.mustCall(() => socket.end()));
  }
  res.end('hello world');
}, 4));

function get(path, callback) {
  return http.get({
    host: 'localhost',
    port: server.address().port,
    agent: agent,
    path: path
  }, callback).on('socket', common.mustCall(checkListeners));
}

function checkDataAndSockets(body) {
  assert.strictEqual(body.toString(), 'hello world');
  assert.strictEqual(agent.sockets[name].length, 1);
  assert.strictEqual(agent.freeSockets[name], undefined);
}

function second() {
  // Request second, use the same socket
  const req = get('/second', common.mustCall((res) => {
    assert.strictEqual(req.reusedSocket, true);
    assert.strictEqual(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(agent.sockets[name].length, 1);
      assert.strictEqual(agent.freeSockets[name], undefined);
      process.nextTick(common.mustCall(() => {
        assert.strictEqual(agent.sockets[name], undefined);
        assert.strictEqual(agent.freeSockets[name].length, 1);
        remoteClose();
      }));
    }));
  }));
}

function remoteClose() {
  // Mock remote server close the socket
  const req = get('/remote_close', common.mustCall((res) => {
    assert.deepStrictEqual(req.reusedSocket, true);
    assert.deepStrictEqual(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(agent.sockets[name].length, 1);
      assert.strictEqual(agent.freeSockets[name], undefined);
      process.nextTick(common.mustCall(() => {
        assert.strictEqual(agent.sockets[name], undefined);
        assert.strictEqual(agent.freeSockets[name].length, 1);
        // Waiting remote server close the socket
        setTimeout(common.mustCall(() => {
          assert.strictEqual(agent.sockets[name], undefined);
          assert.strictEqual(agent.freeSockets[name], undefined);
          remoteError();
        }), common.platformTimeout(200));
      }));
    }));
  }));
}

function remoteError() {
  // Remote server will destroy the socket
  const req = get('/error', common.mustNotCall());
  req.on('error', common.mustCall((err) => {
    assert(err);
    assert.strictEqual(err.message, 'socket hang up');
    assert.strictEqual(agent.sockets[name].length, 1);
    assert.strictEqual(agent.freeSockets[name], undefined);
    // Wait socket 'close' event emit
    setTimeout(common.mustCall(() => {
      assert.strictEqual(agent.sockets[name], undefined);
      assert.strictEqual(agent.freeSockets[name], undefined);
      server.close();
    }), common.platformTimeout(1));
  }));
}

server.listen(0, common.mustCall(() => {
  name = `localhost:${server.address().port}:`;
  // Request first, and keep alive
  const req = get('/first', common.mustCall((res) => {
    assert.strictEqual(req.reusedSocket, false);
    assert.strictEqual(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(agent.sockets[name].length, 1);
      assert.strictEqual(agent.freeSockets[name], undefined);
      process.nextTick(common.mustCall(() => {
        assert.strictEqual(agent.sockets[name], undefined);
        assert.strictEqual(agent.freeSockets[name].length, 1);
        second();
      }));
    }));
  }));
}));

// Check for listener leaks when reusing sockets.
function checkListeners(socket) {
  const callback = common.mustCall(() => {
    if (!socket.destroyed) {
      assert.strictEqual(socket.listenerCount('data'), 0);
      assert.strictEqual(socket.listenerCount('drain'), 0);
      // Sockets have freeSocketErrorListener.
      assert.strictEqual(socket.listenerCount('error'), 1);
      // Sockets have onReadableStreamEnd.
      assert.strictEqual(socket.listenerCount('end'), 1);
    }

    socket.off('free', callback);
    socket.off('close', callback);
  });
  assert.strictEqual(socket.listenerCount('error'), 1);
  assert.strictEqual(socket.listenerCount('end'), 2);
  socket.once('free', callback);
  socket.once('close', callback);
}
