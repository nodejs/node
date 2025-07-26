'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.server.stream.error' channel when
// an error occurs during the processing of a ServerHttp2Stream.

const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');
const { Duplex } = require('stream');

let expectedError = null;

dc.subscribe('http2.server.stream.error', common.mustCall(({ stream, error }) => {
  // Since ServerHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex and the constructor name is
  // 'ServerHttp2Stream'.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ServerHttp2Stream');
  assert.strictEqual(stream.closed, true);
  assert.strictEqual(stream.destroyed, true);

  assert.ok(error);
  assert.strictEqual(error, expectedError);
}));

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expectedError);
  }));

  expectedError = new Error('HTTP/2 server stream error');
  stream.destroy(expectedError);
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const stream = client.request({});
  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');

    client.close();
    server.close();
  }));
}));
