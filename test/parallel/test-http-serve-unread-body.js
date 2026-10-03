'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

let requests = 0;
const server = http.serve(common.mustCall((request) => {
  requests++;
  return new Response(new URL(request.url).pathname);
}, 2));

server.listen(0, common.mustCall(() => {
  const client = net.createConnection(server.address().port, common.mustCall(() => {
    client.write(
      'POST /one HTTP/1.1\r\n' +
      'Host: localhost\r\n' +
      'Content-Length: 4\r\n\r\n' +
      'body' +
      'GET /two HTTP/1.1\r\n' +
      'Host: localhost\r\n' +
      'Connection: close\r\n\r\n',
    );
  }));

  let data = '';
  client.setEncoding('utf8');
  client.on('data', (chunk) => data += chunk);
  client.on('end', common.mustCall(() => {
    assert.strictEqual(requests, 2);
    assert.match(data, /\/one/);
    assert.match(data, /\/two/);
    server.close();
  }));
}));
