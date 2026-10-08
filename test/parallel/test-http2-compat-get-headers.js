'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');

const server = http2.createServer(common.mustCall((request, response) => {
  const empty = response.getHeaders();
  assert.strictEqual(Object.getPrototypeOf(empty), null);
  assert.deepStrictEqual(Object.keys(empty), []);
  assert.notStrictEqual(response.getHeaders(), empty);

  const cookies = ['first=value', 'second=value'];
  response.setHeader('X-First', 'first');
  response.setHeader('10', 'ten');
  response.setHeader('2', 'two');
  response.setHeader('__proto__', 'proto');
  response.setHeader('Constructor', 'constructor');
  response.setHeader('Set-Cookie', cookies);

  const expected = {
    '__proto__': null,
    '2': 'two',
    '10': 'ten',
    'x-first': 'first',
    ['__proto__']: 'proto',
    'constructor': 'constructor',
    'set-cookie': cookies,
  };
  const headers = response.getHeaders();
  assert.deepStrictEqual(headers, expected);
  assert.deepStrictEqual(Object.keys(headers), Object.keys(expected));
  assert.strictEqual(headers['set-cookie'], cookies);
  assert.notStrictEqual(response.getHeaders(), headers);

  headers['x-first'] = 'changed';
  headers['x-new'] = 'new';
  delete headers['2'];
  assert.deepStrictEqual(response.getHeaders(), expected);

  headers['set-cookie'].push('third=value');
  assert.strictEqual(response.getHeader('set-cookie'), cookies);
  assert.strictEqual(response.getHeader('set-cookie').length, 3);

  response.removeHeader('X-First');
  response.setHeader('X-First', 'replacement');
  delete expected['x-first'];
  expected['x-first'] = 'replacement';
  assert.deepStrictEqual(response.getHeaders(), expected);
  assert.deepStrictEqual(Object.keys(response.getHeaders()), Object.keys(expected));

  for (const name of response.getHeaderNames()) {
    response.removeHeader(name);
  }
  assert.deepStrictEqual(response.getHeaders(), empty);
  assert.notStrictEqual(response.getHeaders(), empty);

  response.setHeader('X-Sent', 'value');
  response.flushHeaders();
  assert.deepStrictEqual(response.getHeaders(), {
    '__proto__': null,
    'x-sent': 'value',
    ':status': 200,
  });
  response.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const request = client.request();
  request.resume();
  request.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  request.end();
}));
