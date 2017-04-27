'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const tls = require('tls');
const fs = require('fs');

const tests = [];

const serverOptions = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
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
  const server = https.createServer(serverOptions, function(req, res) {
    // just do nothing, we should get a timeout event.
  });
  server.listen(0, common.mustCall(function() {
    const s = server.setTimeout(50, common.mustCall(function(socket) {
      socket.destroy();
      server.close();
      cb();
    }));
    assert.ok(s instanceof https.Server);
    https.get({
      port: this.address().port,
      rejectUnauthorized: false
    }).on('error', function() {});
  }));
});

test(function serverRequestTimeout(cb) {
  function handler(req, res) {
    // just do nothing, we should get a timeout event.
    req.setTimeout(50, common.mustCall(function() {
      req.socket.destroy();
      server.close();
      cb();
    }));
  }

  const server = https.createServer(serverOptions, common.mustCall(handler));
  server.listen(0, function() {
    const req = https.request({
      port: this.address().port,
      method: 'POST',
      rejectUnauthorized: false
    });
    req.on('error', function() {});
    req.write('Hello');
    // req is in progress
  });
});

test(function serverResponseTimeout(cb) {
  function handler(req, res) {
    // just do nothing, we should get a timeout event.
    res.setTimeout(50, common.mustCall(function() {
      res.socket.destroy();
      server.close();
      cb();
    }));
  }

  const server = https.createServer(serverOptions, common.mustCall(handler));
  server.listen(0, function() {
    https.get({
      port: this.address().port,
      rejectUnauthorized: false
    }).on('error', function() {});
  });
});

test(function serverRequestNotTimeoutAfterEnd(cb) {
  function handler(req, res) {
    // just do nothing, we should get a timeout event.
    req.setTimeout(50, common.mustNotCall());
    res.on('timeout', common.mustCall(function(socket) {}));
  }
  const server = https.createServer(serverOptions, common.mustCall(handler));
  server.on('timeout', function(socket) {
    socket.destroy();
    server.close();
    cb();
  });
  server.listen(0, function() {
    https.get({
      port: this.address().port,
      rejectUnauthorized: false
    }).on('error', function() {});
  });
});

test(function serverResponseTimeoutWithPipeline(cb) {
  let caughtTimeout = '';
  process.on('exit', function() {
    assert.strictEqual(caughtTimeout, '/2');
  });
  const server = https.createServer(serverOptions, function(req, res) {
    res.setTimeout(50, function() {
      caughtTimeout += req.url;
    });
    if (req.url === '/1') res.end();
  });
  server.on('timeout', function(socket) {
    socket.destroy();
    server.close();
    cb();
  });
  server.listen(0, function() {
    const options = {
      port: this.address().port,
      allowHalfOpen: true,
      rejectUnauthorized: false
    };
    const c = tls.connect(options, function() {
      c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    });
  });
});

test(function idleTimeout(cb) {
  const server = https.createServer(serverOptions,
                                    common.mustCall(function(req, res) {
                                      req.on('timeout', common.mustNotCall());
                                      res.on('timeout', common.mustNotCall());
                                      res.end();
                                    }));
  server.setTimeout(50, common.mustCall(function(socket) {
    socket.destroy();
    server.close();
    cb();
  }));
  server.listen(0, function() {
    const options = {
      port: this.address().port,
      allowHalfOpen: true,
      rejectUnauthorized: false
    };
    tls.connect(options, function() {
      this.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      // Keep-Alive
    });
  });
});
