'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// Verify that ServerResponse.writeHead() works with arrays.

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.setHeader('test', '1');
    res.writeHead(200, [ 'test', '2', 'test2', '2' ]);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers.test, '2');
      assert.strictEqual(res.headers.test2, '2');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200, [ 'test', '1', 'test2', '2' ]);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers.test, '1');
      assert.strictEqual(res.headers.test2, '2');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}


{
  const server = http.createServer(common.mustCall((req, res) => {
    try {
      res.writeHead(200, [ 'test', '1', 'test2', '2', 'asd' ]);
    } catch (err) {
      assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
    }
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200, undefined, [ 'foo', 'bar' ]);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusMessage, 'OK');
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers.foo, 'bar');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}
