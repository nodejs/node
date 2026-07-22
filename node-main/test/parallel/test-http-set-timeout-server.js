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
  const server = http.createServer();
  server.listen(common.mustCall(() => {
    const s = server.setTimeout(50, common.mustCall((socket) => {
      socket.destroy();
      server.close();
      cb();
    }));
    assert.ok(s instanceof http.Server);
    http.get({
      port: server.address().port
    }).on('error', common.mustCall());
  }));
});

test(function serverRequestTimeout(cb) {
  const server = http.createServer(common.mustCall((req, res) => {
    // Just do nothing, we should get a timeout event.
    const s = req.setTimeout(50, common.mustCall((socket) => {
      socket.destroy();
      server.close();
      cb();
    }));
    assert.ok(s instanceof http.IncomingMessage);
  }));
  server.listen(common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      method: 'POST'
    });
    req.on('error', common.mustCall());
    req.write('Hello');
    // req is in progress
  }));
});

test(function serverResponseTimeout(cb) {
  const server = http.createServer(common.mustCall((req, res) => {
    // Just do nothing, we should get a timeout event.
    const s = res.setTimeout(50, common.mustCall((socket) => {
      socket.destroy();
      server.close();
      cb();
    }));
    assert.ok(s instanceof http.OutgoingMessage);
  }));
  server.listen(common.mustCall(() => {
    http.get({
      port: server.address().port
    }).on('error', common.mustCall());
  }));
});

test(function serverRequestNotTimeoutAfterEnd(cb) {
  const server = http.createServer(common.mustCall((req, res) => {
    // Just do nothing, we should get a timeout event.
    const s = req.setTimeout(50, common.mustNotCall());
    assert.ok(s instanceof http.IncomingMessage);
    res.on('timeout', common.mustCall());
  }));
  server.on('timeout', common.mustCall((socket) => {
    socket.destroy();
    server.close();
    cb();
  }));
  server.listen(common.mustCall(() => {
    http.get({
      port: server.address().port
    }).on('error', common.mustCall());
  }));
});

test(function serverResponseTimeoutWithPipeline(cb) {
  let caughtTimeout = '';
  let secReceived = false;
  process.on('exit', () => {
    assert.strictEqual(caughtTimeout, '/2');
  });
  const server = http.createServer((req, res) => {
    if (req.url === '/2')
      secReceived = true;
    if (req.url === '/1') {
      res.end();
      return;
    }
    const s = res.setTimeout(50, () => {
      caughtTimeout += req.url;
    });
    assert.ok(s instanceof http.OutgoingMessage);
  });
  server.on('timeout', common.mustCall((socket) => {
    if (secReceived) {
      socket.destroy();
      server.close();
      cb();
    }
  }));
  server.listen(common.mustCall(() => {
    const options = {
      port: server.address().port,
      allowHalfOpen: true,
    };
    const c = net.connect(options, () => {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
  }));
});

test(function idleTimeout(cb) {
  // Test that the an idle connection invokes the timeout callback.
  const server = http.createServer();
  const s = server.setTimeout(50, common.mustCall((socket) => {
    socket.destroy();
    server.close();
    cb();
  }));
  assert.ok(s instanceof http.Server);
  server.listen(common.mustCall(() => {
    const options = {
      port: server.address().port,
      allowHalfOpen: true,
    };
    const c = net.connect(options, () => {
      // ECONNRESET could happen on a heavily-loaded server.
      c.on('error', (e) => {
        if (e.message !== 'read ECONNRESET')
          throw e;
      });
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      // Keep-Alive
    });
  }));
});

test(function fastTimeout(cb) {
  let connectionHandlerInvoked = false;
  let timeoutHandlerInvoked = false;
  let connectionSocket;

  function invokeCallbackIfDone() {
    if (connectionHandlerInvoked && timeoutHandlerInvoked) {
      connectionSocket.destroy();
      server.close();
      cb();
    }
  }

  const server = http.createServer(common.mustCall((req, res) => {
    req.on('timeout', common.mustNotCall());
    res.end();
    connectionHandlerInvoked = true;
    invokeCallbackIfDone();
  }));
  const s = server.setTimeout(1, common.mustCall((socket) => {
    connectionSocket = socket;
    timeoutHandlerInvoked = true;
    invokeCallbackIfDone();
  }));
  assert.ok(s instanceof http.Server);
  server.listen(common.mustCall(() => {
    const options = {
      port: server.address().port,
      allowHalfOpen: true,
    };
    const c = net.connect(options, () => {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      // Keep-Alive
    });
  }));
});
