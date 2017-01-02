'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');

var body = 'hello world\n';

function test(handler, request_generator, response_validator) {
  var server = http.createServer(handler);

  var client_got_eof = false;
  var server_response = '';

  server.listen(0);
  server.on('listening', function() {
    var c = net.createConnection(this.address().port);

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
    return ('GET / HTTP/1.0\r\n' +
        'User-Agent: curl/7.19.7 (x86_64-pc-linux-gnu) libcurl/7.19.7 ' +
        'OpenSSL/0.9.8k zlib/1.2.3.3 libidn/1.15\r\n' +
        'Host: 127.0.0.1:1337\r\n' +
        'Accept: */*\r\n' +
        '\r\n');
  }

  function response_validator(server_response, client_got_eof, timed_out) {
    const expected_response = 'HTTP/1.1 200 OK\r\n' +
                              'Content-Type: text/plain\r\n' +
                              'Connection: close\r\n' +
                              '\r\n' +
                              'Hello, world!';

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
    return 'GET / HTTP/1.1\r\n' +
        'User-Agent: curl/7.19.7 (x86_64-pc-linux-gnu) libcurl/7.19.7 ' +
        'OpenSSL/0.9.8k zlib/1.2.3.3 libidn/1.15\r\n' +
        'Connection: close\r\n' +
        'Host: 127.0.0.1:1337\r\n' +
        'Accept: */*\r\n' +
        '\r\n';
  }

  function response_validator(server_response, client_got_eof, timed_out) {
    const expected_response = 'HTTP/1.1 200 OK\r\n' +
                              'Content-Type: text/plain\r\n' +
                              'Connection: close\r\n' +
                              'Transfer-Encoding: chunked\r\n' +
                              '\r\n' +
                              '7\r\n' +
                              'Hello, \r\n' +
                              '6\r\n' +
                              'world!\r\n' +
                              '0\r\n' +
                              '\r\n';

    assert.strictEqual(expected_response, server_response);
    assert.strictEqual(true, client_got_eof);
    assert.strictEqual(false, timed_out);
  }

  test(handler, request_generator, response_validator);
}
