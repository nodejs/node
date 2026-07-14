'use strict';

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const http = require('node:http');

const body = Buffer.from('A\u{1F642}B');
const raw = [];
const decoded = [];
let clientRequest;
let clientResponse;

dc.subscribe('http.client.response.bodyChunk', common.mustCallAtLeast((message) => {
  const { request, response, chunk } = message;
  assert.strictEqual(request, clientRequest);
  assert.strictEqual(response, clientResponse);
  assert.strictEqual(response.req, request);
  assert(Buffer.isBuffer(chunk));
  raw.push(Buffer.from(chunk));
}));

const server = http.createServer(common.mustCall((request, response) => {
  request.resume();
  response.write(body.subarray(0, 3));
  setImmediate(() => response.end(body.subarray(3)));
}));

server.listen(0, common.mustCall(() => {
  clientRequest = http.get({ port: server.address().port }, common.mustCall((response) => {
    clientResponse = response;
    response.setEncoding('utf8');
    response.on('data', common.mustCallAtLeast((chunk) => {
      assert.strictEqual(typeof chunk, 'string');
      decoded.push(chunk);
    }));
    response.on('end', common.mustCall(() => {
      assert.deepStrictEqual(Buffer.concat(raw), body);
      assert.strictEqual(decoded.join(''), body.toString());
      server.close(common.mustCall());
    }));
  }));
}));
