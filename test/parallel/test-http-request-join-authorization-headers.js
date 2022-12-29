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
