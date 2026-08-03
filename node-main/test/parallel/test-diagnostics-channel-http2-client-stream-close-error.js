'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.client.stream.close' channel when
// a ClientHttp2Stream is destroyed because of an error.

const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');
const { Duplex } = require('stream');

dc.subscribe('http2.client.stream.close', common.mustCall(({ stream }) => {
  // Since ClientHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex and the constructor name is
  // 'ClientHttp2Stream'.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ClientHttp2Stream');
  assert.strictEqual(stream.closed, true);
  assert.strictEqual(stream.destroyed, true);

  assert.strictEqual(stream.rstCode, http2.constants.NGHTTP2_CANCEL);
}));

const server = http2.createServer();
server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const ac = new AbortController();
  const stream = client.request({}, { signal: ac.signal });
  ac.abort();

  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ABORT_ERR');
    assert.strictEqual(err.name, 'AbortError');

    client.close();
    server.close();
  }));
}));
