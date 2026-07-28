'use strict';

const common = require('../common');
const events = require('events');
const { createServer, request } = require('http');
const assert = require('assert');

events.captureRejections = true;

{
  const server = createServer(common.mustCall(async (req, res) => {
    // We will test that this header is cleaned up before forwarding.
    res.setHeader('content-type', 'application/json');
    throw new Error('kaboom');
  }));

  server.listen(0, common.mustCall(() => {
    const req = request({
      method: 'GET',
      host: server.address().host,
      port: server.address().port
    });

    req.end();

    req.on('response', common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 500);
      assert.strictEqual(Object.hasOwn(res.headers, 'content-type'), false);
      let data = '';
      res.setEncoding('utf8');
      res.on('data', common.mustCall((chunk) => {
        data += chunk;
      }));
      res.on('end', common.mustCall(() => {
        assert.strictEqual(data, 'Internal Server Error');
        server.close();
      }));
    }));
  }));
}

{
  let resolve;
  const latch = new Promise((_resolve) => {
    resolve = _resolve;
  });
  const server = createServer(common.mustCall(async (req, res) => {
    server.close();

    // We will test that this header is cleaned up before forwarding.
    res.setHeader('content-type', 'application/json');
    res.write('{');
    req.resume();

    // Wait so the data is on the wire
    await latch;

    throw new Error('kaboom');
  }));

  server.listen(0, common.mustCall(() => {
    const req = request({
      method: 'GET',
      host: server.address().host,
      port: server.address().port
    });

    req.end();

    req.on('response', common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers['content-type'], 'application/json');
      resolve();

      let data = '';
      res.setEncoding('utf8');
      res.on('data', common.mustCall((chunk) => {
        data += chunk;
      }));

      req.on('close', common.mustCall(() => {
        assert.strictEqual(data, '{');
      }));
    }));
  }));
}

{
  const server = createServer(common.mustCall(async (req, res) => {
    // We will test that this header is cleaned up before forwarding.
    res.writeHead(200);
    throw new Error('kaboom');
  }));

  server.listen(0, common.mustCall(() => {
    const req = request({
      method: 'GET',
      host: server.address().host,
      port: server.address().port
    });

    req.end();
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
      server.close();
    }));
  }));
}
