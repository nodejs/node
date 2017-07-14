'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  res.end('ok');
  server.close();
}).listen(0, common.mustCall(() => {
  http.request({
    port: server.address().port,
    encoding: 'utf8'
  }, common.mustCall((res) => {
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', common.mustCall(() => assert.strictEqual(data, 'ok')));
  })).end();
}));
