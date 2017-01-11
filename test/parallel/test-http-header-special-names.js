/* eslint-disable no-proto */
'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const clientHeaders = Object.create(null);
const serverHeaders = Object.create(null);
clientHeaders['__proto__'] = serverHeaders['__proto__'] = 'abc';
clientHeaders['constructor'] = serverHeaders['constructor'] = '123';
clientHeaders['Foo'] = serverHeaders['Foo'] = '456';

const server = http.createServer(common.mustCall((req, res) => {
  const headerNames = Object.keys(req.headers);
  assert.notStrictEqual(headerNames.indexOf('__proto__'), -1);
  assert.notStrictEqual(headerNames.indexOf('constructor'), -1);
  assert.notStrictEqual(headerNames.indexOf('foo'), -1);
  assert.strictEqual(req.headers['__proto__'], 'abc');
  assert.strictEqual(req.headers['constructor'], '123');
  assert.strictEqual(req.headers['foo'], '456');
  res.writeHead(200, serverHeaders);
  res.end();
  server.close();
}));
server.listen(common.PORT, () => {
  http.get({
    port: common.PORT,
    headers: clientHeaders
  }, common.mustCall((res) => {
    const headerNames = Object.keys(res.headers);
    assert.equal(res.statusCode, 200);
    assert.notStrictEqual(headerNames.indexOf('__proto__'), -1);
    assert.notStrictEqual(headerNames.indexOf('constructor'), -1);
    assert.notStrictEqual(headerNames.indexOf('foo'), -1);
    assert.strictEqual(res.headers['__proto__'], 'abc');
    assert.strictEqual(res.headers['constructor'], '123');
    assert.strictEqual(res.headers['foo'], '456');
  }));
});
