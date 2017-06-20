'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const https = require('https');
const http = require('http');

const tls = require('tls');
const fs = require('fs');

const tests = [];

const serverOptions = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
};

function test(fn) {
  if (!tests.length)
    process.nextTick(run);
  tests.push(fn);
}

function run() {
  const fn = tests.shift();
  if (fn) {
    console.log('# %s', fn.name);
    fn(run);
  } else {
    console.log('ok');
  }
}

test(function serverTimeout(cb) {
  const server = https.createServer(
    serverOptions,
    common.mustCall(function(req, res) {
      // just do nothing, we should get a
      // timeout event.
    }));
  server.listen(common.mustCall(function() {
    const s = server.setTimeout(50, common.mustCall(function(socket) {
      socket.destroy();
      server.close();
      cb();
    }));
    assert.ok(s instanceof https.Server);
    https.get({
      port: server.address().port,
      rejectUnauthorized: false
    }).on('error', common.noop);
  }));
});

test(function serverRequestTimeout(cb) {
  const server = https.createServer(
    serverOptions,
    common.mustCall(function(req, res) {
      // just do nothing, we should get a
      // timeout event.
      const s = req.setTimeout(
        50,
        common.mustCall(function(socket) {
          socket.destroy();
          server.close();
          cb();
        }));
      assert.ok(s instanceof http.IncomingMessage);
    }));
  server.listen(common.mustCall(function() {
    const req = https.request({
      port: server.address().port,
      method: 'POST',
      rejectUnauthorized: false
    });
    req.on('error', common.noop);
    req.write('Hello');
    // req is in progress
  }));
});

test(function serverResponseTimeout(cb) {
  const server = https.createServer(
    serverOptions,
    common.mustCall(function(req, res) {
      // just do nothing, we should get a timeout event.
      const s = res.setTimeout(50, common.mustCall(function(socket) {
        socket.destroy();
        server.close();
        cb();
      }));
      assert.ok(s instanceof http.OutgoingMessage);
    }));
  server.listen(common.mustCall(function() {
    https.get({
      port: server.address().port,
      rejectUnauthorized: false
    }).on('error', common.mustCall());
  }));
});

test(function serverRequestNotTimeoutAfterEnd(cb) {
  const server = https.createServer(
    serverOptions,
    common.mustCall(function(req, res) {
      // just do nothing, we should get a timeout event.
      const s = req.setTimeout(50, common.mustNotCall());
      assert.ok(s instanceof http.IncomingMessage);
      res.on('timeout', common.mustCall());
    }));
  server.on('timeout', common.mustCall(function(socket) {
    socket.destroy();
    server.close();
    cb();
  }));
  server.listen(common.mustCall(function() {
    https.get({
      port: server.address().port,
      rejectUnauthorized: false
    }).on('error', common.mustCall());
  }));
});

test(function serverResponseTimeoutWithPipeline(cb) {
  let caughtTimeout = '';
  let secReceived = false;
  process.on('exit', function() {
    assert.strictEqual(caughtTimeout, '/2');
  });
  const server = https.createServer(serverOptions, function(req, res) {
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
    const options = {
      port: server.address().port,
      allowHalfOpen: true,
      rejectUnauthorized: false
    };
    const c = tls.connect(options, function() {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
  }));
});

test(function idleTimeout(cb) {
  const server = https.createServer(
    serverOptions,
    common.mustCall(function(req, res) {
      req.on('timeout', common.mustNotCall());
      res.on('timeout', common.mustNotCall());
      res.end();
    }));
  const s = server.setTimeout(50, common.mustCall(function(socket) {
    socket.destroy();
    server.close();
    cb();
  }));
  assert.ok(s instanceof https.Server);
  server.listen(common.mustCall(function() {
    const options = {
      port: server.address().port,
      allowHalfOpen: true,
      rejectUnauthorized: false
    };
    tls.connect(options, function() {
      this.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      // Keep-Alive
    });
  }));
});
