'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');

const server = http.createServer((req, res) => {
  common.fail('Server accepted a request without a Host header.');
});

server.listen(common.PORT, common.mustCall(() => {
  // Per: RFC7230 -
  // A server MUST respond with a 400 (Bad Request) status code to any
  // HTTP/1.1 request message that lacks a Host header field and to any
  // request message that contains more than one Host header field or a
  // Host header field with an invalid field-value.
  const client = net.connect({port: common.PORT}, common.mustCall(() => {
    // The following request does not contain a Host header. RFC 7230
    // requires that the request be rejected by the server. The only
    // valid response is to receive an error.
    client.write('GET / HTTP/1.1\r\n\r\n');
    client.on('error', () => {
      server.close();
      client.end();
    });
  }));
}));
