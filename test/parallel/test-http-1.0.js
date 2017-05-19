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
const net = require('net');
const http = require('http');

const { tagUnwrap, tagCRLFy } = common;

const userAgent = tagUnwrap`
  curl/7.19.7 (x86_64-pc-linux-gnu) libcurl/7.19.7
  OpenSSL/0.9.8k zlib/1.2.3.3 libidn/1.15
`;

const body = 'hello world\n';

function test(handler, request_generator, response_validator) {
  const server = http.createServer(handler);

  let client_got_eof = false;
  let server_response = '';

  server.listen(0);
  server.on('listening', function() {
    const c = net.createConnection(this.address().port);

    c.setEncoding('utf8');

    c.on('connect', function() {
      c.write(request_generator());
    });

    c.on('data', function(chunk) {
      server_response += chunk;
    });

    c.on('end', common.mustCall(function() {
      client_got_eof = true;
      c.end();
      server.close();
      response_validator(server_response, client_got_eof, false);
    }));
  });
}

{
  function handler(req, res) {
    assert.strictEqual('1.0', req.httpVersion);
    assert.strictEqual(1, req.httpVersionMajor);
    assert.strictEqual(0, req.httpVersionMinor);
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end(body);
  }

  function request_generator() {
    return 'GET / HTTP/1.0\r\n\r\n';
  }

  function response_validator(server_response, client_got_eof, timed_out) {
    const m = server_response.split('\r\n\r\n');
    assert.strictEqual(m[1], body);
    assert.strictEqual(true, client_got_eof);
    assert.strictEqual(false, timed_out);
  }

  test(handler, request_generator, response_validator);
}

//
// Don't send HTTP/1.1 status lines to HTTP/1.0 clients.
//
// https://github.com/joyent/node/issues/1234
//
{
  function handler(req, res) {
    assert.strictEqual('1.0', req.httpVersion);
    assert.strictEqual(1, req.httpVersionMajor);
    assert.strictEqual(0, req.httpVersionMinor);
    res.sendDate = false;
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('Hello, '); res._send('');
    res.write('world!'); res._send('');
    res.end();
  }

  function request_generator() {
    return (tagCRLFy`
      GET / HTTP/1.0
      User-Agent: ${userAgent}
      Host: 127.0.0.1:1337
      Accept: */*


    `);
  }

  function response_validator(server_response, client_got_eof, timed_out) {
    const expected_response = tagCRLFy`
      HTTP/1.1 200 OK
      Content-Type: text/plain
      Connection: close

      Hello, world!
    `;

    assert.strictEqual(expected_response, server_response);
    assert.strictEqual(true, client_got_eof);
    assert.strictEqual(false, timed_out);
  }

  test(handler, request_generator, response_validator);
}

{
  function handler(req, res) {
    assert.strictEqual('1.1', req.httpVersion);
    assert.strictEqual(1, req.httpVersionMajor);
    assert.strictEqual(1, req.httpVersionMinor);
    res.sendDate = false;
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('Hello, '); res._send('');
    res.write('world!'); res._send('');
    res.end();
  }

  function request_generator() {
    return tagCRLFy`
      GET / HTTP/1.1
      User-Agent: ${userAgent}
      Connection: close
      Host: 127.0.0.1:1337
      Accept: */*


    `;
  }

  function response_validator(server_response, client_got_eof, timed_out) {
    const expected_response = tagCRLFy`
      HTTP/1.1 200 OK
      Content-Type: text/plain
      Connection: close
      Transfer-Encoding: chunked

      7
      Hello,${' '}
      6
      world!
      0


    `;

    assert.strictEqual(expected_response, server_response);
    assert.strictEqual(true, client_got_eof);
    assert.strictEqual(false, timed_out);
  }

  test(handler, request_generator, response_validator);
}
