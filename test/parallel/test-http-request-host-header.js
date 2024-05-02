'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  const server = http.createServer(common.mustNotCall((req, res) => {
    res.writeHead(200);
    res.end();
  }));

  // From RFC 7230 5.4 https://datatracker.ietf.org/doc/html/rfc7230#section-5.4
  // A server MUST respond with a 400 (Bad Request) status code to any
  // HTTP/1.1 request message that lacks a Host header field
  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port, headers: [] }, (res) => {
      assert.strictEqual(res.statusCode, 400);
      assert.strictEqual(res.headers.connection, 'close');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}

{
  const server = http.createServer({ requireHostHeader: false }, common.mustCall((req, res) => {
    res.writeHead(200, ['test', '1']);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port, headers: [] }, (res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers.test, '1');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}
