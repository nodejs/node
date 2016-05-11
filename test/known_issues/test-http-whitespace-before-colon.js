'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');

const server = http.createServer((req, res) => {
  common.fail('Server accepted a message with whitespace before colon');
});

server.listen(common.PORT, common.mustCall(() => {
  const client = net.connect({port: common.PORT}, common.mustCall(() => {
    // RFC 7230 *requires* that messages that contain any
    // whitespace between the header field name and the
    // colon are rejected outright. The only valid response
    // to the following request is an error:
    client.write('GET / HTTP/1.1\r\nHost : foo\r\n\r\n');
    client.on('error', () => {
      server.close();
      client.end();
    });
  }));
}));
