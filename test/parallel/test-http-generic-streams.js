'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const MakeDuplexPair = require('../common/duplexpair');

// Test 1: Simple HTTP test, no keep-alive.
{
  const testData = 'Hello, World!\n';
  const server = http.createServer(common.mustCall((req, res) => {
    res.statusCode = 200;
    res.setHeader('Content-Type', 'text/plain');
    res.end(testData);
  }));

  const { clientSide, serverSide } = MakeDuplexPair();
  server.emit('connection', serverSide);

  const req = http.request({
    createConnection: common.mustCall(() => clientSide)
  }, common.mustCall((res) => {
    res.setEncoding('utf8');
    res.on('data', common.mustCall((data) => {
      assert.strictEqual(data, testData);
    }));
  }));
  req.end();
}

// Test 2: Keep-alive for 2 requests.
{
  const testData = 'Hello, World!\n';
  const server = http.createServer(common.mustCall((req, res) => {
    res.statusCode = 200;
    res.setHeader('Content-Type', 'text/plain');
    res.end(testData);
  }, 2));

  const { clientSide, serverSide } = MakeDuplexPair();
  server.emit('connection', serverSide);

  function doRequest(cb) {
    const req = http.request({
      createConnection: common.mustCall(() => clientSide),
      headers: { Connection: 'keep-alive' }
    }, common.mustCall((res) => {
      res.setEncoding('utf8');
      res.on('data', common.mustCall((data) => {
        assert.strictEqual(data, testData);
      }));
      res.on('end', common.mustCall(cb));
    }));
    req.shouldKeepAlive = true;
    req.end();
  }

  doRequest(() => {
    doRequest();
  });
}
