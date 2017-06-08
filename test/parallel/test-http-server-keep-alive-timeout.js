'use strict';

module.exports.testServerKeepAliveTimeout = function(common, isHttps) {
  let http;
  let net;
  let createOptions;
  let serverOptions;
  let createServer;

  if (isHttps) {
    if (!common.hasCrypto) {
      common.skip('missing crypto');
      return;
    }

    http = require('https');
    net = require('tls');

    createOptions = function(server) {
      return {
 port: server.address().port,
 allowHalfOpen: true,
 rejectUnauthorized: false
      };
    };

    const fs = require('fs');
    serverOptions = {
      key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
      cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
    };

    createServer = function(fn) {
      return http.createServer(serverOptions, fn);
    };


  } else {
    http = require('http');
    net = require('net');

    createOptions = function(server) {
      return {
 port: server.address().port,
 allowHalfOpen: true,
      };
    };

    createServer = function(fn) {
      return http.createServer(fn);
    };

  }

  const assert = require('assert');
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

  test(function serverKeepAliveTimeoutWithPipeline(cb) {
    let requestCount = 0;
    process.on('exit', function() {
      assert.strictEqual(requestCount, 3);
    });
    const server = createServer((req, res) => {
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
      const c = net.connect(createOptions(server), () => {
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
    const server = createServer((req, res) => {
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
      const c = net.connect(createOptions(server), () => {
        c.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
        c.write('GET /2 HTTP/1.1\r\nHost: localhost\r\n\r\n');
        c.write('GET /3 HTTP/1.1\r\nHost: localhost\r\n\r\n');
      });
    }));
  });
};


const common = require('../common');
module.exports.testServerKeepAliveTimeout(common, false);
