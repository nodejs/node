'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const tests = [];

function test(fn) {
  if (!tests.length) {
    process.nextTick(run);
  }
  tests.push(fn);
}

function run() {
  const fn = tests.shift();
  if (fn) fn(run);
}

test(function serverEndKeepAliveTimeoutWithPipeline(cb) {
  let socket;
  let destroyedSockets = 0;
  let timeoutCount = 0;
  let requestCount = 0;
  process.on('exit', () => {
    assert.strictEqual(timeoutCount, 1);
    assert.strictEqual(requestCount, 3);
    assert.strictEqual(destroyedSockets, 1);
  });
  const server = http.createServer((req, res) => {
    socket = req.socket;
    requestCount++;
    res.end();
  });
  server.setTimeout(200, (socket) => {
    timeoutCount++;
    socket.destroy();
  });
  server.keepAliveTimeout = 50;
  server.listen(0, common.mustCall(() => {
    const options = {
      port: server.address().port,
      allowHalfOpen: true
    };
    const c = net.connect(options, () => {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
    setTimeout(() => {
      server.close();
      if (socket.destroyed) destroyedSockets++;
      cb();
    }, 1000);
  }));
});

test(function serverNoEndKeepAliveTimeoutWithPipeline(cb) {
  let socket;
  let destroyedSockets = 0;
  let timeoutCount = 0;
  let requestCount = 0;
  process.on('exit', () => {
    assert.strictEqual(timeoutCount, 1);
    assert.strictEqual(requestCount, 3);
    assert.strictEqual(destroyedSockets, 1);
  });
  const server = http.createServer((req, res) => {
    socket = req.socket;
    requestCount++;
  });
  server.setTimeout(200, (socket) => {
    timeoutCount++;
    socket.destroy();
  });
  server.keepAliveTimeout = 50;
  server.listen(0, common.mustCall(() => {
    const options = {
      port: server.address().port,
      allowHalfOpen: true
    };
    const c = net.connect(options, () => {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
    setTimeout(() => {
      server.close();
      if (socket.destroyed) destroyedSockets++;
      cb();
    }, 1000);
  }));
});
