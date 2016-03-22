'use strict';

const common = require('../common');
const net = require('net');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.mustCall((req, res) => {
  // The http-parser tolerates whitespace in the header field name
  // when parsing, but should trim that when it passes it off to
  // the headers.
  assert(req.headers['foo']);
  req.on('data', common.mustCall(() => {}));
  req.on('end', common.mustCall(() => {
    assert(req.trailers['baz']);
    res.end('ok');
    server.close();
  }));
}));
server.listen(common.PORT, common.mustCall(() => {
  // Have to use net client because the HTTP client does not
  // permit sending whitespace in header field names.
  const client = net.connect({port: common.PORT}, common.mustCall(() => {
    client.write('GET / HTTP/1.1\r\n' +
                 'Host: localhost\r\n' +
                 'Connection: close\r\n' +
                 'Foo : test\r\n' +
                 'Trailer: Baz\r\n' +
                 'Transfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n' +
                 'Baz   : test\r\n\r\n');
  }));
  client.on('close', common.mustCall(() => {
    client.end();
  }));
}));
