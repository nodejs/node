'use strict';

const common = require('../common');
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
  const server = http.createServer(common.mustCall((req, res) => {
    res.end();
  }, 3));
  server.setTimeout(500, common.mustCall((socket) => {
    // End this test and call `run()` for the next test (if any).
    socket.destroy();
    server.close();
    cb();
  }));
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
  }));
});

test(function serverNoEndKeepAliveTimeoutWithPipeline(cb) {
  const server = http.createServer(common.mustCall(3));
  server.setTimeout(500, common.mustCall((socket) => {
    // End this test and call `run()` for the next test (if any).
    socket.destroy();
    server.close();
    cb();
  }));
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
  }));
});
