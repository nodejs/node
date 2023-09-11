'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

function shouldThrowOnMoreBytes() {
  const server = http.createServer(common.mustCall((req, res) => {
    res.strictContentLength = true;
    res.setHeader('Content-Length', 5);
    res.write('hello');
    assert.throws(() => {
      res.write('a');
    }, {
      code: 'ERR_HTTP_CONTENT_LENGTH_MISMATCH'
    });
    res.statusCode = 200;
    res.end();
  }));

  server.listen(0, () => {
    const req = http.get({
      port: server.address().port,
    }, common.mustCall((res) => {
      res.resume();
      assert.strictEqual(res.statusCode, 200);
      server.close();
    }));
    req.end();
  });
}

function shouldNotThrow() {
  const server = http.createServer(common.mustCall((req, res) => {
    res.strictContentLength = true;
    res.write('helloaa');
    res.statusCode = 200;
    res.end('ending');
  }));

  server.listen(0, () => {
    http.get({
      port: server.address().port,
    }, common.mustCall((res) => {
      res.resume();
      assert.strictEqual(res.statusCode, 200);
      server.close();
    }));
  });
}


function shouldThrowOnFewerBytes() {
  const server = http.createServer(common.mustCall((req, res) => {
    res.strictContentLength = true;
    res.setHeader('Content-Length', 5);
    res.write('a');
    res.statusCode = 200;
    assert.throws(() => {
      res.end('aaa');
    }, {
      code: 'ERR_HTTP_CONTENT_LENGTH_MISMATCH'
    });
    res.end('aaaa');
  }));

  server.listen(0, () => {
    http.get({
      port: server.address().port,
    }, common.mustCall((res) => {
      res.resume();
      assert.strictEqual(res.statusCode, 200);
      server.close();
    }));
  });
}

shouldThrowOnMoreBytes();
shouldNotThrow();
shouldThrowOnFewerBytes();
