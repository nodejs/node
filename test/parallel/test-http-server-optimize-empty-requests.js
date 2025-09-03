'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

let reqs = 0;
const server = http.createServer({
  optimizeEmptyRequests: true
}, (req, res) => {
  reqs++;
  assert.strictEqual(req._dumped, true);
  assert.strictEqual(req._readableState.ended, true);
  assert.strictEqual(req._readableState.endEmitted, true);
  assert.strictEqual(req._readableState.destroyed, true);

  res.writeHead(200);
  res.end('ok');
});

server.listen(0, common.mustCall(async () => {
  // GET request without Content-Length (should be optimized)
  const getRequest = 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n';
  await makeRequest(getRequest);

  // HEAD request (should always be optimized regardless of headers)
  const headRequest = 'HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n';
  await makeRequest(headRequest);

  // POST request without body headers (should be optimized)
  const postWithoutBodyHeaders = 'POST / HTTP/1.1\r\nHost: localhost\r\n\r\n';
  await makeRequest(postWithoutBodyHeaders);

  // DELETE request without body headers (should be optimized)
  const deleteWithoutBodyHeaders = 'DELETE / HTTP/1.1\r\nHost: localhost\r\n\r\n';
  await makeRequest(deleteWithoutBodyHeaders);
  server.close();

  assert.strictEqual(reqs, 4);
}));

function makeRequest(str) {
  return new Promise((resolve) => {
    const client = net.connect({ port: server.address().port }, common.mustCall(() => {
      client.on('data', common.mustCall());
      client.on('end', common.mustCall(() => {
        resolve();
      }));
      client.write(str);
      client.end();
    }));
  });
}
