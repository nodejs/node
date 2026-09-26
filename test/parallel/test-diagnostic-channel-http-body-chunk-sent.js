'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const dc = require('diagnostics_channel');

// The outgoing body chunk diagnostics channels let tools account for the actual
// bytes being sent without Node paying to measure them when nobody is reading.
// Consumers compute the byte length themselves from `data` and `encoding`.
// Refs: https://github.com/nodejs/node/pull/66039

const requestBodyChunks = [];
dc.subscribe('http.client.request.bodyChunkSent', ({ message, data, encoding }) => {
  assert.ok(message instanceof http.ClientRequest);
  requestBodyChunks.push({ data, encoding });
});

const responseBodyChunks = [];
dc.subscribe('http.server.response.bodyChunkSent', ({ message, data, encoding }) => {
  assert.ok(message instanceof http.ServerResponse);
  responseBodyChunks.push({ data, encoding });
});

const requestBody = 'é'.repeat(50) + '漢'.repeat(20);
const responseBody = 'é'.repeat(100) + '😀'.repeat(10);

function byteLength(data, encoding) {
  return typeof data === 'string' ? Buffer.byteLength(data, encoding) : data.byteLength;
}

const server = http.createServer(common.mustCall((req, res) => {
  req.on('data', () => {});
  req.on('end', common.mustCall(() => {
    res.write('é'.repeat(100));
    res.end('😀'.repeat(10));
  }));
}));

server.listen(0, common.mustCall(() => {
  const { port } = server.address();

  const req = http.request({
    port,
    method: 'POST',
  }, common.mustCall((res) => {
    res.on('data', () => {});
    res.on('end', common.mustCall(() => {
      // Each user-visible body write is published once, carrying the chunk and
      // its encoding, so the byte length can be recovered without Node needing
      // to measure it.
      const requestSent = requestBodyChunks.reduce(
        (total, { data, encoding }) => total + byteLength(data, encoding), 0);
      assert.strictEqual(requestSent, Buffer.byteLength(requestBody));

      const responseSent = responseBodyChunks.reduce(
        (total, { data, encoding }) => total + byteLength(data, encoding), 0);
      assert.strictEqual(responseSent, Buffer.byteLength(responseBody));

      // The published chunks preserve the exact bytes that were written.
      assert.deepStrictEqual(
        requestBodyChunks.map(({ data }) => data).join(''),
        requestBody);
      assert.deepStrictEqual(
        responseBodyChunks.map(({ data }) => data).join(''),
        responseBody);

      server.close();
    }));
  }));

  req.write('é'.repeat(50));
  req.end('漢'.repeat(20));
}));
