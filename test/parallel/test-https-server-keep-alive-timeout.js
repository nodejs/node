'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fs = require('fs');

const tests = [];

const serverOptions = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

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

test(function serverKeepAliveTimeoutWithPipeline(cb) {
  let requestCount = 0;
  process.on('exit', function() {
    assert.strictEqual(requestCount, 3);
  });
  const server = https.createServer(serverOptions, (req, res) => {
    requestCount++;
    res.end();
  });
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
      allowHalfOpen: true,
      rejectUnauthorized: false
    };
    const c = tls.connect(options, () => {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
  }));
});

test(function serverNoEndKeepAliveTimeoutWithPipeline(cb) {
  let requestCount = 0;
  process.on('exit', () => {
    assert.strictEqual(requestCount, 3);
  });
  const server = https.createServer(serverOptions, (req, res) => {
    requestCount++;
  });
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
      allowHalfOpen: true,
      rejectUnauthorized: false
    };
    const c = tls.connect(options, () => {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
  }));
});
