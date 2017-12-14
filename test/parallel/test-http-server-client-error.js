'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');
const net = require('net');

const server = http.createServer(common.mustCall(function(req, res) {
  res.end();
}));

server.on('clientError', common.mustCall(function(err, socket) {
  assert.strictEqual(err instanceof Error, true);
  assert.strictEqual(err.code, 'HPE_INVALID_METHOD');
  assert.strictEqual(err.bytesParsed, 1);
  assert.strictEqual(err.message, 'Parse Error');
  assert.strictEqual(err.rawPacket.toString(), 'Oopsie-doopsie\r\n');

  socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');

  server.close();
}));

server.listen(0, function() {
  function next() {
    // Invalid request
    const client = net.connect(server.address().port);
    client.end('Oopsie-doopsie\r\n');

    let chunks = '';
    client.on('data', function(chunk) {
      chunks += chunk;
    });
    client.once('end', function() {
      assert.strictEqual(chunks, 'HTTP/1.1 400 Bad Request\r\n\r\n');
    });
  }

  // Normal request
  http.get({ port: this.address().port, path: '/' }, function(res) {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.once('end', next);
  });
});
