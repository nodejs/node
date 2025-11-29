'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.client.stream.bodyChunkSent' and
// 'http2.client.stream.bodySent' channels when ClientHttp2Streams bodies are
// being sent with multiple Buffers.

const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');
const { Duplex } = require('stream');

let bodyChunkSent = false;

dc.subscribe('http2.client.stream.bodyChunkSent', common.mustCall(({ stream, writev, data, encoding }) => {
  // Since ClientHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ClientHttp2Stream');

  assert.strictEqual(writev, true);

  assert.ok(Array.isArray(data));
  assert.strictEqual(data.length, 2);

  assert.ok(Buffer.from('foo').equals(data[0]));
  assert.ok(Buffer.from('bar').equals(data[1]));

  assert.strictEqual(encoding, '');

  bodyChunkSent = true;
}));

dc.subscribe('http2.client.stream.bodySent', common.mustCall(({ stream }) => {
  // 'http2.client.stream.bodyChunkSent' must run first.
  assert.ok(bodyChunkSent);

  // Since ClientHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ClientHttp2Stream');
}));

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond({}, { endStream: true });
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const stream = client.request({ [http2.constants.HTTP2_HEADER_METHOD]: 'POST' });
  stream.write(Buffer.from('foo'));
  stream.write(Buffer.from('bar'));
  stream.end();

  stream.on('response', common.mustCall(() => {
    client.close();
    server.close();
  }));
}, 1));
