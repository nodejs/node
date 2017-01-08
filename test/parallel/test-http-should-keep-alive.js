'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const SERVER_RESPONSES = [
  'HTTP/1.0 200 ok\r\nContent-Length: 0\r\n\r\n',
  'HTTP/1.0 200 ok\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n',
  'HTTP/1.0 200 ok\r\nContent-Length: 0\r\nConnection: close\r\n\r\n',
  'HTTP/1.1 200 ok\r\nContent-Length: 0\r\n\r\n',
  'HTTP/1.1 200 ok\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n',
  'HTTP/1.1 200 ok\r\nContent-Length: 0\r\nConnection: close\r\n\r\n'
];
const SHOULD_KEEP_ALIVE = [
  false, // HTTP/1.0, default
  true,  // HTTP/1.0, Connection: keep-alive
  false, // HTTP/1.0, Connection: close
  true,  // HTTP/1.1, default
  true,  // HTTP/1.1, Connection: keep-alive
  false  // HTTP/1.1, Connection: close
];
let requests = 0;
let responses = 0;
http.globalAgent.maxSockets = 5;

const server = net.createServer(function(socket) {
  socket.write(SERVER_RESPONSES[requests]);
  ++requests;
}).listen(0, function() {
  function makeRequest() {
    const req = http.get({port: server.address().port}, function(res) {
      assert.equal(req.shouldKeepAlive, SHOULD_KEEP_ALIVE[responses],
                   SERVER_RESPONSES[responses] + ' should ' +
                   (SHOULD_KEEP_ALIVE[responses] ? '' : 'not ') +
                   'Keep-Alive');
      ++responses;
      if (responses < SHOULD_KEEP_ALIVE.length) {
        makeRequest();
      } else {
        server.close();
      }
      res.resume();
    });
  }

  makeRequest();
});

process.on('exit', function() {
  assert.equal(requests, SERVER_RESPONSES.length);
  assert.equal(responses, SHOULD_KEEP_ALIVE.length);
});
