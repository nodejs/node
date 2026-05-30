'use strict';

const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');

// Regression test for https://github.com/nodejs/node/issues/45150
// Writing an odd-length hex string to a stream that batches writes via
// Writev (e.g. HTTP requests that are automatically corked) used to
// fatal-assert in StringBytes::StorageSize. The trailing incomplete nibble
// should be silently dropped, consistent with non-Writev paths.

// Test 1: HTTP POST with a single odd-length hex write.
// "1" has no complete bytes in hex encoding, so the request body is empty.
{
  const server = http.createServer(common.mustCall((req, res) => {
    const chunks = [];
    req.on('data', (chunk) => chunks.push(chunk));
    req.on('end', common.mustCall(() => {
      assert.strictEqual(Buffer.concat(chunks).length, 0);
      res.end();
      server.close();
    }));
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      method: 'POST',
    }, common.mustCall((res) => {
      res.resume();
    }));
    req.write('1', 'hex');
    req.end();
  }));
}

// Test 2: HTTP POST with cork/uncork and mixed odd-length hex writes.
// "ff1" (3 hex chars) decodes to 1 byte (0xff); the trailing "1" nibble is
// dropped.  "1" (1 hex char) decodes to 0 bytes.
{
  const server = http.createServer(common.mustCall((req, res) => {
    const chunks = [];
    req.on('data', (chunk) => chunks.push(chunk));
    req.on('end', common.mustCall(() => {
      assert.deepStrictEqual(Buffer.concat(chunks), Buffer.from([0xff]));
      res.end();
      server.close();
    }));
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      method: 'POST',
    }, common.mustCall((res) => {
      res.resume();
    }));
    req.cork();
    req.write('ff1', 'hex');
    req.write('1', 'hex');
    req.uncork();
    req.end();
  }));
}

// Test 3: net socket with cork/uncork and an odd-length hex write.
// Exercises the Writev path directly at the net layer.
{
  const server = net.createServer(common.mustCall((socket) => {
    const chunks = [];
    socket.on('data', (chunk) => chunks.push(chunk));
    socket.on('end', common.mustCall(() => {
      assert.deepStrictEqual(Buffer.concat(chunks), Buffer.from([0xff]));
      server.close();
    }));
    socket.resume();
  }));

  server.listen(0, common.mustCall(() => {
    const conn = net.createConnection(server.address().port);
    conn.on('connect', common.mustCall(() => {
      conn.cork();
      conn.write('ff', 'hex');
      conn.write('1', 'hex');
      conn.uncork();
      conn.end();
    }));
  }));
}
