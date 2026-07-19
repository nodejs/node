'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

// Verify that arbitrary characters after a \r cannot be used to perform HTTP request smuggling attacks.

{
  const server = http.createServer(common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    let response = '';

    client.on('data', common.mustCall((chunk) => {
      response += chunk;
    }));

    client.setEncoding('utf8');
    client.on('error', common.mustNotCall());
    client.on('end', common.mustCall(() => {
      assert.strictEqual(
        response,
        'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n'
      );
      server.close();
    }));

    client.write('' +
      'GET / HTTP/1.1\r\n' +
      'Connection: close\r\n' +
      'Host: a\r\n\rZ\r\n' + // Note the Z at the end of the line instead of a \n
      'GET /evil: HTTP/1.1\r\n' +
      'Host: a\r\n\r\n'
    );

    client.resume();
  }));
}

{
  const server = http.createServer((request, response) => {
    // Since chunk parsing failed, none of this should be called

    request.on('data', common.mustNotCall());
    request.on('end', common.mustNotCall());
  });

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    let response = '';

    client.on('data', common.mustCall((chunk) => {
      response += chunk;
    }));

    client.setEncoding('utf8');
    client.on('error', common.mustNotCall());
    client.on('end', common.mustCall(() => {
      assert.strictEqual(
        response,
        'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n'
      );
      server.close();
    }));

    client.write('' +
      'GET / HTTP/1.1\r\n' +
      'Host: a\r\n' +
      'Connection: close \r\n' +
      'Transfer-Encoding: chunked \r\n' +
      '\r\n' +
      '5\r\r;ABCD\r\n' + // Note the second \r instead of \n after the chunk length
      '34\r\n' +
      'E\r\n' +
      '0\r\n' +
      '\r\n' +
      'GET / HTTP/1.1 \r\n' +
      'Host: a\r\n' +
      'Content-Length: 5\r\n' +
      '\r\n' +
      '0\r\n' +
      '\r\n'
    );

    client.resume();
  }));
}
