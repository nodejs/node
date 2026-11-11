'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const assert = require('assert');
const http2 = require('http2');

// Integration tests for the http2.connect() `httpValidation` option.
// See also test-http2-util-headers-list.js for the per-value validation logic.

const server = http2.createServer();
// Consume any stream that is actually created. Control characters and
// duplicate pseudo-headers may be rejected by nghttp2 asynchronously, so
// swallow any stream errors rather than letting them become uncaught.
server.on('stream', (stream) => {
  stream.on('error', () => {});
});

server.listen(0, common.mustCall(() => {
  const origin = `http://localhost:${server.address().port}`;

  function swallow(stream) {
    stream.on('error', () => {});
    stream.destroy();
    return stream;
  }

  // -- strict (default) --
  {
    const session = http2.connect(origin);
    session.on('error', () => {});
    // Non-latin1 characters are rejected by default.
    assert.throws(() => session.request({ 'x-bad-char': 'oʊmɪɡə' }), {
      code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
    });
    // Control characters are rejected by default per the HTTP RFCs.
    assert.throws(() => session.request({ 'x-control': 'value\x01here' }), {
      code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
    });
    // strictSingleValueFields is enabled by default.
    assert.throws(() => session.request([
      [':method', 'GET'],
      [':method', 'POST'],
    ]), {
      code: 'ERR_HTTP2_HEADER_SINGLE_VALUE',
    });
    // The :path pseudo-header must not contain unescaped characters.
    assert.throws(() => session.request({ ':path': 'bad\u0100path' }), {
      code: 'ERR_UNESCAPED_CHARACTERS',
    });
    session.close();
  }

  // -- relaxed --
  {
    const session = http2.connect(origin, { httpValidation: 'relaxed' });
    session.on('error', () => {});
    // Control characters permitted by the Fetch specification are allowed.
    swallow(session.request({ 'x-control': 'value\x01here' }));
    // relaxed/insecure mode subsumes and disables strictSingleValueFields.
    swallow(session.request([
      [':method', 'GET'],
      [':method', 'POST'],
    ]));
    // Non-latin1 characters are still rejected in relaxed mode.
    assert.throws(() => session.request({ 'x-bad-char': 'oʊmɪɡə' }), {
      code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
    });
    // NUL is always rejected.
    assert.throws(() => session.request({ 'x-nul': 'value\x00here' }), {
      code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
    });
    // The :path pseudo-header check is always enforced.
    assert.throws(() => session.request({ ':path': 'bad\u0100path' }), {
      code: 'ERR_UNESCAPED_CHARACTERS',
    });
    session.close();
  }

  // -- insecure --
  {
    const session = http2.connect(origin, { httpValidation: 'insecure' });
    session.on('error', () => {});
    // All header value validation is skipped, including newlines.
    swallow(session.request({ 'x-newline': 'value\nhere' }));
    // strictSingleValueFields is disabled in insecure mode too.
    swallow(session.request([
      [':method', 'GET'],
      [':method', 'POST'],
    ]));
    // The :path pseudo-header check is still enforced.
    assert.throws(() => session.request({ ':path': 'bad\u0100path' }), {
      code: 'ERR_UNESCAPED_CHARACTERS',
    });
    session.close();
  }

  // -- invalid option value --
  {
    assert.throws(() => http2.connect(origin, { httpValidation: 'bogus' }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }

  server.close();
}));
