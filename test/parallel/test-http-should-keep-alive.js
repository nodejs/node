'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const Countdown = require('../common/countdown');

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
http.globalAgent.maxSockets = 5;

const countdown = new Countdown(SHOULD_KEEP_ALIVE.length, () => server.close());

const getCountdownIndex = () => SERVER_RESPONSES.length - countdown.remaining;

const server = net.createServer(function(socket) {
  socket.write(SERVER_RESPONSES[getCountdownIndex()]);
}).listen(0, function() {
  function makeRequest() {
    const req = http.get({port: server.address().port}, function(res) {
      assert.strictEqual(
        req.shouldKeepAlive, SHOULD_KEEP_ALIVE[getCountdownIndex()],
        `${SERVER_RESPONSES[getCountdownIndex()]} should ${
          SHOULD_KEEP_ALIVE[getCountdownIndex()] ? '' : 'not '}Keep-Alive`);
      countdown.dec();
      if (countdown.remaining) {
        makeRequest();
      }
      res.resume();
    });
  }
  makeRequest();
});
