'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

let reqs = 0;
let optimizedReqs = 0;
const server = http.createServer({
  optimizeEmptyRequests: true
}, common.mustCallAtLeast((req, res) => {
  reqs++;
  if (req._dumped) {
    optimizedReqs++;
    req.on('data', common.mustNotCall());
    req.on('end', common.mustNotCall());

    assert.strictEqual(req._dumped, true);
    assert.strictEqual(req.readableEnded, true);
    assert.strictEqual(req.destroyed, true);
  }
  res.writeHead(200);
  res.end('ok');
}));

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

  // POST request with Content-Length header (should not be optimized)
  const postWithContentLength = 'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n';
  await makeRequest(postWithContentLength);

  // GET request with Content-Length header (should not be optimized)
  const getWithContentLength = 'GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n';
  await makeRequest(getWithContentLength);

  // POST request with Transfer-Encoding header (should not be optimized)
  const postWithTransferEncoding = 'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n';
  await makeRequest(postWithTransferEncoding);

  // GET request with Transfer-Encoding header (should not be optimized)
  const getWithTransferEncoding = 'GET / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n';
  await makeRequest(getWithTransferEncoding);

  server.close();

  assert.strictEqual(reqs, 8, `Expected 8 requests but got ${reqs}`);
  assert.strictEqual(optimizedReqs, 4, `Expected 4 optimized requests but got ${optimizedReqs}`);
}));

function makeRequest(str) {
  return new Promise((resolve) => {
    const client = net.connect({ port: server.address().port }, common.mustCall(() => {
      client.on('data', () => {});
      client.on('end', common.mustCall(() => {
        resolve();
      }));
      client.write(str);
      client.end();
    }));
  });
}
