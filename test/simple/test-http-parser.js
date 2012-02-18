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

var HTTPParser = process.binding('http_parser').HTTPParser;

var CRLF = '\r\n';
var REQUEST = HTTPParser.REQUEST;
var RESPONSE = HTTPParser.RESPONSE;

// The purpose of this test is not to check HTTP compliance but to test the
// binding. Tests for pathological http messages should be submitted
// upstream to https://github.com/joyent/http-parser for inclusion into
// deps/http-parser/test.c


function newParser(type) {
  var parser = new HTTPParser(type);

  parser.headers = [];
  parser.url = '';

  parser.onHeaders = function(headers, url) {
    parser.headers = parser.headers.concat(headers);
    parser.url += url;
  };

  parser.onHeadersComplete = function(info) {
  };

  parser.onBody = function(b, start, len) {
    assert.ok(false, 'Function should not be called.');
  };

  parser.onMessageComplete = function() {
  };

  return parser;
}


function mustCall(f, times) {
  var actual = 0;

  process.setMaxListeners(256);
  process.on('exit', function() {
    assert.equal(actual, times || 1);
  });

  return function() {
    actual++;
    return f.apply(this, Array.prototype.slice.call(arguments));
  };
}


function expectBody(expected) {
  return mustCall(function(buf, start, len) {
    var body = '' + buf.slice(start, start + len);
    assert.equal(body, expected);
  });
}


//
// Simple request test.
//
(function() {
  var request = Buffer(
      'GET /hello HTTP/1.1' + CRLF +
      CRLF);

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'GET');
    assert.equal(info.url || parser.url, '/hello');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
  });

  parser.execute(request, 0, request.length);

  //
  // Check that if we throw an error in the callbacks that error will be
  // thrown from parser.execute()
  //

  parser.onHeadersComplete = function(info) {
    throw new Error('hello world');
  };

  parser.reinitialize(HTTPParser.REQUEST);

  assert.throws(function() {
    parser.execute(request, 0, request.length);
  }, Error, 'hello world');
})();


//
// Simple response test.
//
(function() {
  var request = Buffer(
      'HTTP/1.1 200 OK' + CRLF +
      'Content-Type: text/plain' + CRLF +
      'Content-Length: 4' + CRLF +
      CRLF +
      'pong');

  var parser = newParser(RESPONSE);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, undefined);
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
    assert.equal(info.statusCode, 200);
  });

  parser.onBody = mustCall(function(buf, start, len) {
    var body = '' + buf.slice(start, start + len);
    assert.equal(body, 'pong');
  });

  parser.execute(request, 0, request.length);
})();


//
// Response with no headers.
//
(function() {
  var request = Buffer(
      'HTTP/1.0 200 Connection established' + CRLF +
      CRLF);

  var parser = newParser(RESPONSE);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, undefined);
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 0);
    assert.equal(info.statusCode, 200);
    assert.deepEqual(info.headers || parser.headers, []);
  });

  parser.execute(request, 0, request.length);
})();


//
// Trailing headers.
//
(function() {
  var request = Buffer(
      'POST /it HTTP/1.1' + CRLF +
      'Transfer-Encoding: chunked' + CRLF +
      CRLF +
      '4' + CRLF +
      'ping' + CRLF +
      '0' + CRLF +
      'Vary: *' + CRLF +
      'Content-Type: text/plain' + CRLF +
      CRLF);

  var seen_body = false;

  function onHeaders(headers, url) {
    assert.ok(seen_body); // trailers should come after the body
    assert.deepEqual(headers,
        ['Vary', '*', 'Content-Type', 'text/plain']);
  }

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'POST');
    assert.equal(info.url || parser.url, '/it');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
    // expect to see trailing headers now
    parser.onHeaders = mustCall(onHeaders);
  });

  parser.onBody = mustCall(function(buf, start, len) {
    var body = '' + buf.slice(start, start + len);
    assert.equal(body, 'ping');
    seen_body = true;
  });

  parser.execute(request, 0, request.length);
})();


//
// Test header ordering.
//
(function() {
  var request = Buffer(
      'GET / HTTP/1.0' + CRLF +
      'X-Filler: 1337' + CRLF +
      'X-Filler:   42' + CRLF +
      'X-Filler2:  42' + CRLF +
      CRLF);

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'GET');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 0);
    assert.deepEqual(info.headers || parser.headers,
        ['X-Filler', '1337',
         'X-Filler', '42',
         'X-Filler2', '42']);
  });

  parser.execute(request, 0, request.length);
})();


//
// Test large number of headers
//
(function() {
  // 256 X-Filler headers
  var lots_of_headers = 'X-Filler: 42' + CRLF;
  for (var i = 0; i < 8; ++i) lots_of_headers += lots_of_headers;

  var request = Buffer(
      'GET /foo/bar/baz?quux=42#1337 HTTP/1.0' + CRLF +
      lots_of_headers +
      CRLF);

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'GET');
    assert.equal(info.url || parser.url, '/foo/bar/baz?quux=42#1337');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 0);

    var headers = info.headers || parser.headers;

    assert.equal(headers.length, 2 * 256); // 256 key/value pairs
    for (var i = 0; i < headers.length; i += 2) {
      assert.equal(headers[i], 'X-Filler');
      assert.equal(headers[i + 1], '42');
    }
  });

  parser.execute(request, 0, request.length);
})();


//
// Test request body
//
(function() {
  var request = Buffer(
      'POST /it HTTP/1.1' + CRLF +
      'Content-Type: application/x-www-form-urlencoded' + CRLF +
      'Content-Length: 15' + CRLF +
      CRLF +
      'foo=42&bar=1337');

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'POST');
    assert.equal(info.url || parser.url, '/it');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
  });

  parser.onBody = mustCall(function(buf, start, len) {
    var body = '' + buf.slice(start, start + len);
    assert.equal(body, 'foo=42&bar=1337');
  });

  parser.execute(request, 0, request.length);
})();


//
// Test chunked request body
//
(function() {
  var request = Buffer(
      'POST /it HTTP/1.1' + CRLF +
      'Content-Type: text/plain' + CRLF +
      'Transfer-Encoding: chunked' + CRLF +
      CRLF +
      '3' + CRLF +
      '123' + CRLF +
      '6' + CRLF +
      '123456' + CRLF +
      'A' + CRLF +
      '1234567890' + CRLF +
      '0' + CRLF);

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'POST');
    assert.equal(info.url || parser.url, '/it');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
  });

  var body_part = 0,
      body_parts = ['123', '123456', '1234567890'];

  function onBody(buf, start, len) {
    var body = '' + buf.slice(start, start + len);
    assert.equal(body, body_parts[body_part++]);
  }

  parser.onBody = mustCall(onBody, body_parts.length);
  parser.execute(request, 0, request.length);
})();


//
// Test chunked request body spread over multiple buffers (packets)
//
(function() {
  var request = Buffer(
      'POST /it HTTP/1.1' + CRLF +
      'Content-Type: text/plain' + CRLF +
      'Transfer-Encoding: chunked' + CRLF +
      CRLF +
      '3' + CRLF +
      '123' + CRLF +
      '6' + CRLF +
      '123456' + CRLF);

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'POST');
    assert.equal(info.url || parser.url, '/it');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
  });

  var body_part = 0,
      body_parts = [
        '123', '123456', '123456789',
        '123456789ABC', '123456789ABCDEF'];

  function onBody(buf, start, len) {
    var body = '' + buf.slice(start, start + len);
    assert.equal(body, body_parts[body_part++]);
  }

  parser.onBody = mustCall(onBody, body_parts.length);
  parser.execute(request, 0, request.length);

  request = Buffer(
      '9' + CRLF +
      '123456789' + CRLF +
      'C' + CRLF +
      '123456789ABC' + CRLF +
      'F' + CRLF +
      '123456789ABCDEF' + CRLF +
      '0' + CRLF);

  parser.execute(request, 0, request.length);
})();


//
// Stress test.
//
(function() {
  var request = Buffer(
      'POST /helpme HTTP/1.1' + CRLF +
      'Content-Type: text/plain' + CRLF +
      'Transfer-Encoding: chunked' + CRLF +
      CRLF +
      '3' + CRLF +
      '123' + CRLF +
      '6' + CRLF +
      '123456' + CRLF +
      '9' + CRLF +
      '123456789' + CRLF +
      'C' + CRLF +
      '123456789ABC' + CRLF +
      'F' + CRLF +
      '123456789ABCDEF' + CRLF +
      '0' + CRLF);

  function test(a, b) {
    var parser = newParser(REQUEST);

    parser.onHeadersComplete = mustCall(function(info) {
      assert.equal(info.method, 'POST');
      assert.equal(info.url || parser.url, '/helpme');
      assert.equal(info.versionMajor, 1);
      assert.equal(info.versionMinor, 1);
    });

    var expected_body = '123123456123456789123456789ABC123456789ABCDEF';

    parser.onBody = function(buf, start, len) {
      var chunk = '' + buf.slice(start, start + len);
      assert.equal(expected_body.indexOf(chunk), 0);
      expected_body = expected_body.slice(chunk.length);
    };

    parser.execute(a, 0, a.length);
    parser.execute(b, 0, b.length);

    assert.equal(expected_body, '');
  }

  for (var i = 1; i < request.length - 1; ++i) {
    var a = request.slice(0, i);
    console.error('request.slice(0, ' + i + ') = ',
                  JSON.stringify(a.toString()));
    var b = request.slice(i);
    console.error('request.slice(' + i + ') = ',
                  JSON.stringify(b.toString()));
    test(a, b);
  }
})();


//
// Byte by byte test.
//
(function() {
  var request = Buffer(
      'POST /it HTTP/1.1' + CRLF +
      'Content-Type: text/plain' + CRLF +
      'Transfer-Encoding: chunked' + CRLF +
      CRLF +
      '3' + CRLF +
      '123' + CRLF +
      '6' + CRLF +
      '123456' + CRLF +
      '9' + CRLF +
      '123456789' + CRLF +
      'C' + CRLF +
      '123456789ABC' + CRLF +
      'F' + CRLF +
      '123456789ABCDEF' + CRLF +
      '0' + CRLF);

  var parser = newParser(REQUEST);

  parser.onHeadersComplete = mustCall(function(info) {
    assert.equal(info.method, 'POST');
    assert.equal(info.url || parser.url, '/it');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
    assert.deepEqual(info.headers || parser.headers,
        ['Content-Type', 'text/plain',
         'Transfer-Encoding', 'chunked']);
  });

  var expected_body = '123123456123456789123456789ABC123456789ABCDEF';

  parser.onBody = function(buf, start, len) {
    var chunk = '' + buf.slice(start, start + len);
    assert.equal(expected_body.indexOf(chunk), 0);
    expected_body = expected_body.slice(chunk.length);
  };

  for (var i = 0; i < request.length; ++i) {
    parser.execute(request, i, 1);
  }

  assert.equal(expected_body, '');
})();


//
// Test parser reinit sequence.
//
(function() {
  var req1 = Buffer(
      'PUT /this HTTP/1.1' + CRLF +
      'Content-Type: text/plain' + CRLF +
      'Transfer-Encoding: chunked' + CRLF +
      CRLF +
      '4' + CRLF +
      'ping' + CRLF +
      '0' + CRLF);

  var req2 = Buffer(
      'POST /that HTTP/1.0' + CRLF +
      'Content-Type: text/plain' + CRLF +
      'Content-Length: 4' + CRLF +
      CRLF +
      'pong');

  function onHeadersComplete1(info) {
    assert.equal(info.method, 'PUT');
    assert.equal(info.url, '/this');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 1);
    assert.deepEqual(info.headers,
        ['Content-Type', 'text/plain',
         'Transfer-Encoding', 'chunked']);
  };

  function onHeadersComplete2(info) {
    assert.equal(info.method, 'POST');
    assert.equal(info.url, '/that');
    assert.equal(info.versionMajor, 1);
    assert.equal(info.versionMinor, 0);
    assert.deepEqual(info.headers,
        ['Content-Type', 'text/plain',
         'Content-Length', '4']);
  };

  var parser = newParser(REQUEST);
  parser.onHeadersComplete = onHeadersComplete1;
  parser.onBody = expectBody('ping');
  parser.execute(req1, 0, req1.length);

  parser.reinitialize(REQUEST);
  parser.onBody = expectBody('pong');
  parser.onHeadersComplete = onHeadersComplete2;
  parser.execute(req2, 0, req2.length);
})();
