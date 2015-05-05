'use strict';
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

var server = http.createServer(function(req, res) {
  if (req.url === '/error') {
    res.destroy();
    return;
  } else if (req.url === '/remote_close') {
    // cache the socket, close it after 100ms
    var socket = res.connection;
    setTimeout(function() {
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

var name = 'localhost:' + common.PORT + ':';

function checkDataAndSockets(body) {
  assert.equal(body.toString(), 'hello world');
  assert.equal(agent.sockets[name].length, 1);
  assert.equal(agent.freeSockets[name], undefined);
}

function second() {
  // request second, use the same socket
  get('/second', function(res) {
    assert.equal(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', function() {
      assert.equal(agent.sockets[name].length, 1);
      assert.equal(agent.freeSockets[name], undefined);
      process.nextTick(function() {
        assert.equal(agent.sockets[name], undefined);
        assert.equal(agent.freeSockets[name].length, 1);
        remoteClose();
      });
    });
  });
}

function remoteClose() {
  // mock remote server close the socket
  get('/remote_close', function(res) {
    assert.deepEqual(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', function() {
      assert.equal(agent.sockets[name].length, 1);
      assert.equal(agent.freeSockets[name], undefined);
      process.nextTick(function() {
        assert.equal(agent.sockets[name], undefined);
        assert.equal(agent.freeSockets[name].length, 1);
        // waitting remote server close the socket
        setTimeout(function() {
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
  var req = get('/error', function(res) {
    throw new Error('should not call this function');
  });
  req.on('error', function(err) {
    assert.ok(err);
    assert.equal(err.message, 'socket hang up');
    assert.equal(agent.sockets[name].length, 1);
    assert.equal(agent.freeSockets[name], undefined);
    // Wait socket 'close' event emit
    setTimeout(function() {
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
  get('/first', function(res) {
    assert.equal(res.statusCode, 200);
    res.on('data', checkDataAndSockets);
    res.on('end', function() {
      assert.equal(agent.sockets[name].length, 1);
      assert.equal(agent.freeSockets[name], undefined);
      process.nextTick(function() {
        assert.equal(agent.sockets[name], undefined);
        assert.equal(agent.freeSockets[name].length, 1);
        second();
      });
    });
  });
});
