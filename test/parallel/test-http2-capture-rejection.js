'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const events = require('events');
const { createServer, connect } = require('http2');

events.captureRejections = true;

{
  // Test error thrown in the server 'stream' event,
  // after a respond()

  const server = createServer();
  server.on('stream', common.mustCall(async (stream) => {
    server.close();

    stream.respond({ ':status': 200 });

    const _err = new Error('kaboom');
    stream.on('error', common.mustCall((err) => {
      assert.strictEqual(err, _err);
    }));
    throw _err;
  }));

  server.listen(0, common.mustCall(() => {
    const { port } = server.address();
    const session = connect(`http://localhost:${port}`);

    const req = session.request();

    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');
    }));

    req.on('close', common.mustCall(() => {
      session.close();
    }));
  }));
}

{
  // Test error thrown in the server 'stream' event,
  // before a respond().

  const server = createServer();
  server.on('stream', common.mustCall(async (stream) => {
    server.close();

    stream.on('error', common.mustNotCall());

    throw new Error('kaboom');
  }));

  server.listen(0, common.mustCall(() => {
    const { port } = server.address();
    const session = connect(`http://localhost:${port}`);

    const req = session.request();

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 500);
    }));

    req.on('close', common.mustCall(() => {
      session.close();
    }));
  }));
}

{
  // Test error thrown in 'request' event

  const server = createServer(common.mustCall(async (req, res) => {
    server.close();
    res.setHeader('content-type', 'application/json');
    const _err = new Error('kaboom');
    throw _err;
  }));

  server.listen(0, common.mustCall(() => {
    const { port } = server.address();
    const session = connect(`http://localhost:${port}`);

    const req = session.request();

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 500);
      assert.strictEqual(Object.hasOwnProperty.call(headers, 'content-type'),
                         false);
    }));

    req.on('close', common.mustCall(() => {
      session.close();
    }));

    req.resume();
  }));
}

{
  // Test error thrown in the client 'stream' event

  const server = createServer();
  server.on('stream', common.mustCall(async (stream) => {
    const { port } = server.address();

    server.close();

    stream.pushStream({
      ':scheme': 'http',
      ':path': '/foobar',
      ':authority': `localhost:${port}`,
    }, common.mustCall((err, push) => {
      push.respond({
        'content-type': 'text/html',
        ':status': 200
      });
      push.end('pushed by the server');

      stream.end('test');
    }));

    stream.respond({
      ':status': 200
    });
  }));

  server.listen(0, common.mustCall(() => {
    const { port } = server.address();
    const session = connect(`http://localhost:${port}`);

    const req = session.request();
    req.resume();

    session.on('stream', common.mustCall(async (stream) => {
      session.close();

      const _err = new Error('kaboom');
      stream.on('error', common.mustCall((err) => {
        assert.strictEqual(err, _err);
      }));
      throw _err;
    }));

    req.end();
  }));
}
