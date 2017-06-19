'use strict';
const common = require('../common');

const assert = require('assert');
const http = require('http');
const net = require('net');

const tests = [];

function test(fn) {
  if (!tests.length)
    process.nextTick(run);
  tests.push(common.mustCall(fn));
}

function run() {
  const fn = tests.shift();
  if (fn) {
    fn(run);
  }
}

test(function serverTimeout(cb) {
  const server = http.createServer(function(req, res) {
    // just do nothing, we should get a timeout event.
  });
  server.listen(common.mustCall(function() {
    http.get({ port: server.address().port }).on('error', common.mustCall());
  }));
  const s = server.setTimeout(50, common.mustCall(function(socket) {
    socket.destroy();
    server.close();
    cb();
  }));
  assert.ok(s instanceof http.Server);
});

test(function serverRequestTimeout(cb) {
  const server = http.createServer(function(req, res) {
    // just do nothing, we should get a timeout event.
    const s = req.setTimeout(50, common.mustCall(function(socket) {
      socket.destroy();
      server.close();
      cb();
    }));
    assert.ok(s instanceof http.IncomingMessage);
  });
  server.listen(common.mustCall(function() {
    const port = server.address().port;
    const req = http.request({ port: port, method: 'POST' });
    req.on('error', common.mustCall());
    req.write('Hello');
    // req is in progress
  }));
});

test(function serverResponseTimeout(cb) {
  const server = http.createServer(function(req, res) {
    // just do nothing, we should get a timeout event.
    const s = res.setTimeout(50, common.mustCall(function(socket) {
      socket.destroy();
      server.close();
      cb();
    }));
    assert.ok(s instanceof http.OutgoingMessage);
  });
  server.listen(common.mustCall(function() {
    const port = server.address().port;
    http.get({ port: port }).on('error', common.mustCall());
  }));
});

test(function serverRequestNotTimeoutAfterEnd(cb) {
  const server = http.createServer(function(req, res) {
    // just do nothing, we should get a timeout event.
    const s = req.setTimeout(50, common.mustNotCall());
    assert.ok(s instanceof http.IncomingMessage);
    res.on('timeout', common.mustCall());
  });
  server.on('timeout', function(socket) {
    socket.destroy();
    server.close();
    cb();
  });
  server.listen(common.mustCall(function() {
    const port = server.address().port;
    http.get({ port: port }).on('error', common.mustCall());
  }));
});

test(function serverResponseTimeoutWithPipeline(cb) {
  let caughtTimeout = '';
  let secReceived = false;
  process.on('exit', function() {
    assert.strictEqual(caughtTimeout, '/2');
  });
  const server = http.createServer(function(req, res) {
    if (req.url === '/2')
      secReceived = true;
    const s = res.setTimeout(50, function() {
      caughtTimeout += req.url;
    });
    assert.ok(s instanceof http.OutgoingMessage);
    if (req.url === '/1') res.end();
  });
  server.on('timeout', function(socket) {
    if (secReceived) {
      socket.destroy();
      server.close();
      cb();
    }
  });
  server.listen(common.mustCall(function() {
    const port = server.address().port;
    const c = net.connect({ port: port, allowHalfOpen: true }, function() {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
  }));
});

test(function idleTimeout(cb) {
  const server = http.createServer(function(req, res) {
    req.on('timeout', common.mustNotCall());
    res.on('timeout', common.mustNotCall());
    res.end();
  });
  const s = server.setTimeout(50, common.mustCall(function(socket) {
    socket.destroy();
    server.close();
    cb();
  }));
  assert.ok(s instanceof http.Server);
  server.listen(common.mustCall(function() {
    const port = server.address().port;
    const c = net.connect({ port: port, allowHalfOpen: true }, function() {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      // Keep-Alive
    });
  }));
});
