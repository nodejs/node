'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Like http1's IncomingMessage, destroying a compat request before it was
// read to the end destroys the underlying Http2Stream. Destroying it after
// 'end' leaves the stream alone, so the response can still be sent.

const assert = require('assert');
const http2 = require('http2');

const {
  NGHTTP2_NO_ERROR,
  NGHTTP2_INTERNAL_ERROR,
  NGHTTP2_CANCEL,
} = http2.constants;

// Starts a server with the given handler and sends one POST request with a
// body that is never ended. Expects the stream to be reset with `code`.
function testReset(code, handler) {
  const server = http2.createServer(handler);
  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({ ':method': 'POST' });
    req.write('partial');
    req.on('response', common.mustNotCall());
    req.on('error', () => {});
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.rstCode, code);
      client.close();
      server.close();
    }));
  }));
}

// Starts a server with the given handler and sends one complete POST request.
// Expects a complete 200 response with the body 'ok'.
function testResponse(handler) {
  const server = http2.createServer(handler);
  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({ ':method': 'POST' });
    req.end('body');
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
    }));
    let data = '';
    req.setEncoding('utf8');
    req.on('data', (chunk) => { data += chunk; });
    req.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'ok');
    }));
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.rstCode, NGHTTP2_NO_ERROR);
      client.close();
      server.close();
    }));
  }));
}

// destroy() before the request ended destroys the stream.
testReset(NGHTTP2_NO_ERROR, common.mustCall((req, res) => {
  req.on('end', common.mustNotCall());
  req.on('error', common.mustNotCall());
  req.on('aborted', common.mustCall());
  req.on('close', common.mustCall());
  res.on('close', common.mustCall());

  assert.strictEqual(req.destroy(), req);
  assert.strictEqual(req.destroyed, true);
  assert.strictEqual(req.stream.destroyed, true);

  // Destroying it again does nothing.
  req.destroy(new Error('ignored'));
}));

// destroy(err) before the request ended destroys the stream with the error
// and emits it on the request.
{
  const err = new Error('kaboom');
  testReset(NGHTTP2_INTERNAL_ERROR, common.mustCall((req, res) => {
    req.on('end', common.mustNotCall());
    req.on('error', common.mustCall((actual) => {
      assert.strictEqual(actual, err);
    }));
    req.on('close', common.mustCall());
    res.on('close', common.mustCall());

    req.destroy(err);
    assert.strictEqual(req.stream.destroyed, true);
  }));
}

// destroy(err) without an 'error' listener on the request does not throw.
testReset(NGHTTP2_INTERNAL_ERROR, common.mustCall((req, res) => {
  req.on('close', common.mustCall());
  req.destroy(new Error('kaboom'));
  assert.strictEqual(req.stream.destroyed, true);
}));

// destroy() while the stream is closing does not emit 'close' twice.
testReset(NGHTTP2_NO_ERROR, common.mustCall((req, res) => {
  req.on('close', common.mustCall());
  req.stream.destroy();
  req.destroy();
}));

// destroy() after 'end' leaves the stream open.
testResponse(common.mustCall((req, res) => {
  req.resume();
  req.on('aborted', common.mustNotCall());
  req.on('close', common.mustCall());
  req.on('end', common.mustCall(() => {
    req.destroy();
    assert.strictEqual(req.destroyed, true);
    assert.strictEqual(req.stream.destroyed, false);
    setImmediate(() => res.end('ok'));
  }));
}));

// destroy(err) after 'end' emits the error on the request only.
{
  const err = new Error('kaboom');
  testResponse(common.mustCall((req, res) => {
    req.resume();
    req.on('error', common.mustCall((actual) => {
      assert.strictEqual(actual, err);
    }));
    req.on('close', common.mustCall());
    req.on('end', common.mustCall(() => {
      req.destroy(err);
      assert.strictEqual(req.stream.destroyed, false);
      setImmediate(() => res.end('ok'));
    }));
  }));
}

// Reading the whole request with for await...of, which destroys the request
// when it is done, does not abort the response.
testResponse(common.mustCall(async (req, res) => {
  req.on('aborted', common.mustNotCall());
  req.on('close', common.mustCall());
  let body = '';
  req.setEncoding('utf8');
  for await (const chunk of req)
    body += chunk;
  assert.strictEqual(body, 'body');
  assert.strictEqual(req.stream.destroyed, false);
  res.end('ok');
}));

// Leaving for await...of early destroys the request with an AbortError,
// which resets the stream with NGHTTP2_CANCEL.
testReset(NGHTTP2_CANCEL, common.mustCall(async (req, res) => {
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.destroyed, true);
  }));
  // eslint-disable-next-line no-unused-vars
  for await (const chunk of req)
    break;
}));
