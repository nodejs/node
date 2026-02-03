'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer((req, res) => {
  res.writeHead(200, { 'a-header': 'a-header-value' }).end('abc');
});

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall((res) => {
    assert.strictEqual(res.headers['a-header'], 'a-header-value');

    const chunks = [];

    res.on('data', (chunk) => chunks.push(chunk));
    res.on('end', common.mustCall(() => {
      assert.strictEqual(Buffer.concat(chunks).toString(), 'abc');
      server.close();
    }));
  }));
}));
