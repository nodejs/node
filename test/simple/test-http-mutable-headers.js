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

var common = require('../common');
var assert = require('assert');
var http = require('http');

// Simple test of Node's HTTP Client mutable headers
// OutgoingMessage.prototype.setHeader(name, value)
// OutgoingMessage.prototype.getHeader(name)
// OutgoingMessage.prototype.removeHeader(name, value)
// ServerResponse.prototype.statusCode
// <ClientRequest>.method
// <ClientRequest>.path

var testsComplete = 0;
var test = 'headers';
var content = 'hello world\n';
var cookies = [
  'session_token=; path=/; expires=Sun, 15-Sep-2030 13:48:52 GMT',
  'prefers_open_id=; path=/; expires=Thu, 01-Jan-1970 00:00:00 GMT'
];

var s = http.createServer(function(req, res) {
  switch (test) {
    case 'headers':
      assert.throws(function () { res.setHeader() });
      assert.throws(function () { res.setHeader('someHeader') });
      assert.throws(function () { res.getHeader() });
      assert.throws(function () { res.removeHeader() });

      res.setHeader('x-test-header', 'testing');
      res.setHeader('X-TEST-HEADER2', 'testing');
      res.setHeader('set-cookie', cookies);
      res.setHeader('x-test-array-header', [1, 2, 3]);

      var val1 = res.getHeader('x-test-header');
      var val2 = res.getHeader('x-test-header2');
      assert.equal(val1, 'testing');
      assert.equal(val2, 'testing');

      res.removeHeader('x-test-header2');
      break;

    case 'contentLength':
      res.setHeader('content-length', content.length);
      assert.equal(content.length, res.getHeader('Content-Length'));
      break;

    case 'transferEncoding':
      res.setHeader('transfer-encoding', 'chunked');
      assert.equal(res.getHeader('Transfer-Encoding'), 'chunked');
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

s.listen(common.PORT, nextTest);


function nextTest () {
  if (test === 'end') {
    return s.close();
  }

  var bufferedResponse = '';

  http.get({ port: common.PORT }, function(response) {
    console.log('TEST: ' + test);
    console.log('STATUS: ' + response.statusCode);
    console.log('HEADERS: ');
    console.dir(response.headers);

    switch (test) {
      case 'headers':
        assert.equal(response.statusCode, 201);
        assert.equal(response.headers['x-test-header'],
                     'testing');
        assert.equal(response.headers['x-test-array-header'],
                     [1,2,3].join(', '));
        assert.deepEqual(cookies,
                         response.headers['set-cookie']);
        assert.equal(response.headers['x-test-header2'] !== undefined, false);
        // Make the next request
        test = 'contentLength';
        console.log('foobar');
        break;

      case 'contentLength':
        assert.equal(response.headers['content-length'], content.length);
        test = 'transferEncoding';
        break;

      case 'transferEncoding':
        assert.equal(response.headers['transfer-encoding'], 'chunked');
        test = 'writeHead';
        break;

      case 'writeHead':
        assert.equal(response.headers['x-foo'], 'bar');
        assert.equal(response.headers['x-bar'], 'baz');
        assert.equal(200, response.statusCode);
        test = 'end';
        break;

      default:
        throw Error("?");
    }

    response.setEncoding('utf8');
    response.on('data', function(s) {
      bufferedResponse += s;
    });

    response.on('end', function() {
      assert.equal(content, bufferedResponse);
      testsComplete++;
      nextTest();
    });
  });
}


process.on('exit', function() {
  assert.equal(4, testsComplete);
});

