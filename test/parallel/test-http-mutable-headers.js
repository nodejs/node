'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

// Simple test of Node's HTTP Client mutable headers
// OutgoingMessage.prototype.setHeader(name, value)
// OutgoingMessage.prototype.getHeader(name)
// OutgoingMessage.prototype.removeHeader(name, value)
// ServerResponse.prototype.statusCode
// <ClientRequest>.method
// <ClientRequest>.path

let testsComplete = 0;
let test = 'headers';
const content = 'hello world\n';
const cookies = [
  'session_token=; path=/; expires=Sun, 15-Sep-2030 13:48:52 GMT',
  'prefers_open_id=; path=/; expires=Thu, 01-Jan-1970 00:00:00 GMT'
];

const s = http.createServer(function(req, res) {
  switch (test) {
    case 'headers':
      assert.throws(function() { res.setHeader(); });
      assert.throws(function() { res.setHeader('someHeader'); });
      assert.throws(function() { res.getHeader(); });
      assert.throws(function() { res.removeHeader(); });

      res.setHeader('x-test-header', 'testing');
      res.setHeader('X-TEST-HEADER2', 'testing');
      res.setHeader('set-cookie', cookies);
      res.setHeader('x-test-array-header', [1, 2, 3]);

      const val1 = res.getHeader('x-test-header');
      const val2 = res.getHeader('x-test-header2');
      assert.strictEqual(val1, 'testing');
      assert.strictEqual(val2, 'testing');

      res.removeHeader('x-test-header2');
      break;

    case 'contentLength':
      res.setHeader('content-length', content.length);
      assert.strictEqual(content.length, res.getHeader('Content-Length'));
      break;

    case 'transferEncoding':
      res.setHeader('transfer-encoding', 'chunked');
      assert.strictEqual(res.getHeader('Transfer-Encoding'), 'chunked');
      break;

    case 'writeHead':
      res.statusCode = 404;
      res.setHeader('x-foo', 'keyboard cat');
      res.writeHead(200, { 'x-foo': 'bar', 'x-bar': 'baz' });
      break;
  }

  res.statusCode = 201;
  res.end(content);
});

s.listen(0, nextTest);


function nextTest() {
  if (test === 'end') {
    return s.close();
  }

  let bufferedResponse = '';

  http.get({ port: s.address().port }, function(response) {
    console.log('TEST: ' + test);
    console.log('STATUS: ' + response.statusCode);
    console.log('HEADERS: ');
    console.dir(response.headers);

    switch (test) {
      case 'headers':
        assert.strictEqual(response.statusCode, 201);
        assert.strictEqual(response.headers['x-test-header'],
                           'testing');
        assert.strictEqual(response.headers['x-test-array-header'],
                           [1, 2, 3].join(', '));
        assert.deepStrictEqual(cookies,
                               response.headers['set-cookie']);
        assert.strictEqual(response.headers['x-test-header2'] !== undefined,
                           false);
        // Make the next request
        test = 'contentLength';
        console.log('foobar');
        break;

      case 'contentLength':
        assert.strictEqual(response.headers['content-length'],
                           content.length.toString());
        test = 'transferEncoding';
        break;

      case 'transferEncoding':
        assert.strictEqual(response.headers['transfer-encoding'], 'chunked');
        test = 'writeHead';
        break;

      case 'writeHead':
        assert.strictEqual(response.headers['x-foo'], 'bar');
        assert.strictEqual(response.headers['x-bar'], 'baz');
        assert.strictEqual(200, response.statusCode);
        test = 'end';
        break;

      default:
        throw new Error('?');
    }

    response.setEncoding('utf8');
    response.on('data', function(s) {
      bufferedResponse += s;
    });

    response.on('end', function() {
      assert.strictEqual(content, bufferedResponse);
      testsComplete++;
      nextTest();
    });
  });
}


process.on('exit', function() {
  assert.strictEqual(4, testsComplete);
});
