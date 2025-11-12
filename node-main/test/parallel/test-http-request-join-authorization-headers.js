'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  const server = http.createServer({
    requireHostHeader: false,
    joinDuplicateHeaders: true
  }, common.mustCall((req, res) => {
    assert.strictEqual(req.headers.authorization, '1, 2');
    assert.strictEqual(req.headers.cookie, 'foo; bar');
    res.writeHead(200, ['authorization', '3', 'authorization', '4', 'cookie', 'foo', 'cookie', 'bar']);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({
      port: server.address().port,
      headers: ['authorization', '1', 'authorization', '2', 'cookie', 'foo', 'cookie', 'bar'],
      joinDuplicateHeaders: true
    }, (res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers.authorization, '3, 4');
      assert.strictEqual(res.headers.cookie, 'foo; bar');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}

{
  // Server joinDuplicateHeaders false
  const server = http.createServer({
    requireHostHeader: false,
    joinDuplicateHeaders: false
  }, common.mustCall((req, res) => {
    assert.strictEqual(req.headers.authorization, '1'); // non joined value
    res.writeHead(200, ['authorization', '3', 'authorization', '4']);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({
      port: server.address().port,
      headers: ['authorization', '1', 'authorization', '2'],
      joinDuplicateHeaders: true
    }, (res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers.authorization, '3, 4');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}

{
  // Client joinDuplicateHeaders false
  const server = http.createServer({
    requireHostHeader: false,
    joinDuplicateHeaders: true
  }, common.mustCall((req, res) => {
    assert.strictEqual(req.headers.authorization, '1, 2');
    res.writeHead(200, ['authorization', '3', 'authorization', '4']);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({
      port: server.address().port,
      headers: ['authorization', '1', 'authorization', '2'],
      joinDuplicateHeaders: false
    }, (res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers.authorization, '3'); // non joined value
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}
