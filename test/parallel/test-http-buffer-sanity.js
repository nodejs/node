'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const bufferSize = 5 * 1024 * 1024;
let measuredSize = 0;

const buffer = Buffer.allocUnsafe(bufferSize);
for (let i = 0; i < buffer.length; i++) {
  buffer[i] = i % 256;
}

const server = http.Server(function(req, res) {
  server.close();

  let i = 0;

  req.on('data', (d) => {
    measuredSize += d.length;
    for (let j = 0; j < d.length; j++) {
      assert.strictEqual(buffer[i], d[j]);
      i++;
    }
  });

  req.on('end', common.mustCall(() => {
    assert.strictEqual(bufferSize, measuredSize);
    res.writeHead(200);
    res.write('thanks');
    res.end();
  }));
});

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    method: 'POST',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, common.mustCall((res) => {
    res.setEncoding('utf8');
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', common.mustCall(() => {
      assert.strictEqual('thanks', data);
    }));
  }));
  req.end(buffer);
}));
