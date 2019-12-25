// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// Simple test of Node's HTTP Client mutable headers
// OutgoingMessage.prototype.setHeader(name, value)
// OutgoingMessage.prototype.getHeader(name)
// OutgoingMessage.prototype.removeHeader(name, value)
// ServerResponse.prototype.statusCode
// <ClientRequest>.method
// <ClientRequest>.path

let test = 'headers';
const content = 'hello world\n';
const cookies = [
  'session_token=; path=/; expires=Sun, 15-Sep-2030 13:48:52 GMT',
  'prefers_open_id=; path=/; expires=Thu, 01-Jan-1970 00:00:00 GMT'
];

const s = http.createServer(common.mustCall((req, res) => {
  switch (test) {
    case 'headers':
      // Check that header-related functions work before setting any headers
      const headers = res.getHeaders();
      const exoticObj = Object.create(null);
      assert.deepStrictEqual(headers, exoticObj);
      assert.deepStrictEqual(res.getHeaderNames(), []);
      assert.deepStrictEqual(res.hasHeader('Connection'), false);
      assert.deepStrictEqual(res.getHeader('Connection'), undefined);

      assert.throws(
        () => res.setHeader(),
        {
          code: 'ERR_INVALID_HTTP_TOKEN',
          name: 'TypeError',
          message: 'Header name must be a valid HTTP token ["undefined"]'
        }
      );
      assert.throws(
        () => res.setHeader('someHeader'),
        {
          code: 'ERR_HTTP_INVALID_HEADER_VALUE',
          name: 'TypeError',
          message: 'Invalid value "undefined" for header "someHeader"'
        }
      );
      assert.throws(
        () => res.getHeader(),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          name: 'TypeError',
          message: 'The "name" argument must be of type string. ' +
                   'Received undefined'
        }
      );
      assert.throws(
        () => res.removeHeader(),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          name: 'TypeError',
          message: 'The "name" argument must be of type string. ' +
                   'Received undefined'
        }
      );

      const arrayValues = [1, 2, 3];
      res.setHeader('x-test-header', 'testing');
      res.setHeader('X-TEST-HEADER2', 'testing');
      res.setHeader('set-cookie', cookies);
      res.setHeader('x-test-array-header', arrayValues);

      assert.strictEqual(res.getHeader('x-test-header'), 'testing');
      assert.strictEqual(res.getHeader('x-test-header2'), 'testing');

      const headersCopy = res.getHeaders();
      const expected = {
        'x-test-header': 'testing',
        'x-test-header2': 'testing',
        'set-cookie': cookies,
        'x-test-array-header': arrayValues
      };
      Object.setPrototypeOf(expected, null);
      assert.deepStrictEqual(headersCopy, expected);

      assert.deepStrictEqual(res.getHeaderNames(),
                             ['x-test-header', 'x-test-header2',
                              'set-cookie', 'x-test-array-header']);

      assert.strictEqual(res.hasHeader('x-test-header2'), true);
      assert.strictEqual(res.hasHeader('X-TEST-HEADER2'), true);
      assert.strictEqual(res.hasHeader('X-Test-Header2'), true);
      [
        undefined,
        null,
        true,
        {},
        { toString: () => 'X-TEST-HEADER2' },
        () => { }
      ].forEach((val) => {
        assert.throws(
          () => res.hasHeader(val),
          {
            code: 'ERR_INVALID_ARG_TYPE',
            name: 'TypeError',
            message: 'The "name" argument must be of type string.' +
                     common.invalidArgTypeHelper(val)
          }
        );
      });

      res.removeHeader('x-test-header2');

      assert.strictEqual(res.hasHeader('x-test-header2'), false);
      assert.strictEqual(res.hasHeader('X-TEST-HEADER2'), false);
      assert.strictEqual(res.hasHeader('X-Test-Header2'), false);
      break;

    case 'contentLength':
      res.setHeader('content-length', content.length);
      assert.strictEqual(res.getHeader('Content-Length'), content.length);
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

    default:
      assert.fail('Unknown test');
  }

  res.statusCode = 201;
  res.end(content);
}, 4));

s.listen(0, nextTest);


function nextTest() {
  if (test === 'end') {
    return s.close();
  }

  let bufferedResponse = '';

  http.get({ port: s.address().port }, common.mustCall((response) => {
    switch (test) {
      case 'headers':
        assert.strictEqual(response.statusCode, 201);
        assert.strictEqual(response.headers['x-test-header'], 'testing');
        assert.strictEqual(response.headers['x-test-array-header'],
                           [1, 2, 3].join(', '));
        assert.deepStrictEqual(cookies, response.headers['set-cookie']);
        assert.strictEqual(response.headers['x-test-header2'], undefined);
        test = 'contentLength';
        break;

      case 'contentLength':
        assert.strictEqual(+response.headers['content-length'], content.length);
        test = 'transferEncoding';
        break;

      case 'transferEncoding':
        assert.strictEqual(response.headers['transfer-encoding'], 'chunked');
        test = 'writeHead';
        break;

      case 'writeHead':
        assert.strictEqual(response.headers['x-foo'], 'bar');
        assert.strictEqual(response.headers['x-bar'], 'baz');
        assert.strictEqual(response.statusCode, 200);
        test = 'end';
        break;

      default:
        assert.fail('Unknown test');
    }

    response.setEncoding('utf8');
    response.on('data', (s) => {
      bufferedResponse += s;
    });

    response.on('end', common.mustCall(() => {
      assert.strictEqual(bufferedResponse, content);
      common.mustCall(nextTest)();
    }));
  }));
}
