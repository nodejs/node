'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const response = Buffer.from('HTTP/1.1 200 OK\r\n' +
  'Content-Length: 6\r\n' +
  'Transfer-Encoding: Chunked\r\n' +
  '\r\n' +
  '6\r\nfoobar' +
  '0\r\n');

const server = net.createServer(common.mustCall((conn) => {
  conn.write(response);
}));

server.listen(0, common.mustCall(() => {
  const req = http.get(`http://localhost:${server.address().port}/`);
  req.end();
  req.on('error', common.mustCall((err) => {
    const reason = "Transfer-Encoding can't be present with Content-Length";
    assert.strictEqual(err.message, `Parse Error: ${reason}`);
    assert(err.bytesParsed < response.length);
    assert(err.bytesParsed >= response.indexOf('Transfer-Encoding'));
    assert.strictEqual(err.code, 'HPE_INVALID_TRANSFER_ENCODING');
    assert.strictEqual(err.reason, reason);
    assert.deepStrictEqual(err.rawPacket, response);

    server.close();
  }));
}));
