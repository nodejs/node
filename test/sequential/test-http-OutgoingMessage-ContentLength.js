'use strict';
const common = require('../common');

// This test checks if the message is equal or smaller than the
// Content-length in the http header in case of being specified.

const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'Content-Length': 120
  });
  res.write('Lorem ipsum dolor sit amet,');
  res.write(' consectetur elit. ')
  res.end('Quisque blandit orci ligula, ullamcorper elementum sem egestas.');
}));
server.listen(0, () => {
  http.get({
    port: server.address().port,
    headers: { 'Test': 'OutgoingMessage' }
  }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    server.close();
  }));
});
