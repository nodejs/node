'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const body =
  '<html><head></head><body><h1>this is some data</h2></body></html>';

const server = http2.createServer((req, res) => {
  assert.strictEqual(req.headers['x-powered-by'], undefined);
  assert.strictEqual(req.headers.foobar, undefined);
  assert.strictEqual(req.headers['x-h2-header'], undefined);
  assert.strictEqual(req.headers['x-h2-header-2'], undefined);
  assert.strictEqual(req.headers['x-h2-header-3'], undefined);
  assert.strictEqual(req.headers['x-h2-header-4'], undefined);
  res.writeHead(200);
  res.end(body);
});

const server2 = http2.createServer({ strictFieldWhitespaceValidation: false }, (req, res) => {
  assert.strictEqual(req.headers.foobar, 'baz ');
  assert.strictEqual(req.headers['x-powered-by'], 'node-test\t');
  assert.strictEqual(req.headers['x-h2-header'], '\tconnection-test');
  assert.strictEqual(req.headers['x-h2-header-2'], ' connection-test');
  assert.strictEqual(req.headers['x-h2-header-3'], 'connection-test ');
  assert.strictEqual(req.headers['x-h2-header-4'], 'connection-test\t');
  res.writeHead(200);
  res.end(body);
});

server.listen(0, common.mustCall(() => {
  server2.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const client2 = http2.connect(`http://localhost:${server2.address().port}`);
    const headers = {
      'foobar': 'baz ',
      ':path': '/',
      'x-powered-by': 'node-test\t',
      'x-h2-header': '\tconnection-test',
      'x-h2-header-2': ' connection-test',
      'x-h2-header-3': 'connection-test ',
      'x-h2-header-4': 'connection-test\t'
    };
    const req = client.request(headers);

    req.setEncoding('utf8');
    req.on('response', common.mustCall(function(headers) {
      assert.strictEqual(headers[':status'], 200);
    }));

    let data = '';
    req.on('data', (d) => data += d);
    req.on('end', () => {
      assert.strictEqual(body, data);
      client.close();
      client.on('close', common.mustCall(() => {
        server.close();
      }));

      const req2 = client2.request(headers);
      let data2 = '';
      req2.setEncoding('utf8');
      req2.on('response', common.mustCall(function(headers) {
        assert.strictEqual(headers[':status'], 200);
      }));
      req2.on('data', (d) => data2 += d);
      req2.on('end', () => {
        assert.strictEqual(body, data2);
        client2.close();
        client2.on('close', common.mustCall(() => {
          server2.close();
        }));
      });
      req2.end();
    });

    req.end();
  }));
}));

server.on('error', common.mustNotCall());
