'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');


{
  const server = http.createServer({ requireHostHeader: false }, common.mustCall((req, res) => {
    res.setHeaders(['foo', '1', 'bar', '2' ]);
    res.writeHead(200);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port, headers: [] }, (res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers.foo, '1');
      assert.strictEqual(res.headers.bar, '2');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}

{
  const server = http.createServer({ requireHostHeader: false }, common.mustCall((req, res) => {
    res.setHeaders({
      foo: '1',
      bar: '2'
    });
    res.writeHead(200);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, (res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers.foo, '1');
      assert.strictEqual(res.headers.bar, '2');
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}

{
  const server = http.createServer({ requireHostHeader: false }, common.mustCall((req, res) => {
    res.writeHead(200); // headers already sent
    assert.throws(() => {
      res.setHeaders({
        foo: 'bar',
      });
    }, {
      code: 'ERR_HTTP_HEADERS_SENT'
    });
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, (res) => {
      assert.strictEqual(res.headers.foo, undefined);
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}
