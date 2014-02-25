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
var http = require('http');
var Agent = require('_http_agent').Agent;
var EventEmitter = require('events').EventEmitter;

var agent = new Agent({
  keepAlive: true,
  keepAliveMsecs: 1000,
  maxSockets: 5,
  maxFreeSockets: 5
});

var server = http.createServer(function (req, res) {
  if (req.url === '/error') {
    res.destroy();
    return;
  } else if (req.url === '/remote_close') {
    // cache the socket, close it after 100ms
    var socket = res.connection;
    setTimeout(function () {
      socket.end();
    }, 100);
  }
  res.end('hello world');
});

function get(path, callback) {
  return http.get({
    host: 'localhost',
    port: common.PORT,
    agent: agent,
    path: path
  }, callback);
}

var name = 'localhost:' + common.PORT + '::';

function checkDataAndSockets(body) {
  assert.equal(body.toString(), 'hello world');
  assert.equal(agent.sockets[name].length, 1);
  assert.equal(agent.freeSockets[name], undefined);
}

function second() {
  // request second, use the same socket
  get('/second', function (res) {
    assert.equal(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', function () {
      assert.equal(agent.sockets[name].length, 1);
      assert.equal(agent.freeSockets[name], undefined);
      process.nextTick(function () {
        assert.equal(agent.sockets[name], undefined);
        assert.equal(agent.freeSockets[name].length, 1);
        remoteClose();
      });
    });
  });
}

function remoteClose() {
  // mock remote server close the socket
  get('/remote_close', function (res) {
    assert.deepEqual(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', function () {
      assert.equal(agent.sockets[name].length, 1);
      assert.equal(agent.freeSockets[name], undefined);
      process.nextTick(function () {
        assert.equal(agent.sockets[name], undefined);
        assert.equal(agent.freeSockets[name].length, 1);
        // waitting remote server close the socket
        setTimeout(function () {
          assert.equal(agent.sockets[name], undefined);
          assert.equal(agent.freeSockets[name], undefined,
            'freeSockets is not empty');
          remoteError();
        }, 200);
      });
    });
  });
}

function remoteError() {
  // remove server will destroy ths socket
  var req = get('/error', function (res) {
    throw new Error('should not call this function');
  });
  req.on('error', function (err) {
    assert.ok(err);
    assert.equal(err.message, 'socket hang up');
    assert.equal(agent.sockets[name].length, 1);
    assert.equal(agent.freeSockets[name], undefined);
    // Wait socket 'close' event emit
    setTimeout(function () {
      assert.equal(agent.sockets[name], undefined);
      assert.equal(agent.freeSockets[name], undefined);
      done();
    }, 1);
  });
}

function done() {
  console.log('http keepalive agent test success.');
  process.exit(0);
}

server.listen(common.PORT, function() {
  // request first, and keep alive
  get('/first', function (res) {
    assert.equal(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', function () {
      assert.equal(agent.sockets[name].length, 1);
      assert.equal(agent.freeSockets[name], undefined);
      process.nextTick(function () {
        assert.equal(agent.sockets[name], undefined);
        assert.equal(agent.freeSockets[name].length, 1);
        second();
      });
    });
  });
});
