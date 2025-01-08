'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { writeFile } = require('fs').promises;
const http2 = require('http2');
const net = require('net');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const file = tmpdir.resolve('large-file');

const windowSize = 2 ** 16 - 1;
const largeBuffer = new Uint8Array(windowSize * 2);

const {
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    name: 'Error',
    message: 'Stream closed with error code NGHTTP2_INTERNAL_ERROR'
  }));
  // Do not use process.execPath because on shared builds it can be less than window size!
  stream.respondWithFile(file, {
    [HTTP2_HEADER_CONTENT_TYPE]: 'application/octet-stream'
  });
}));

(async () => {
  await writeFile(file, largeBuffer);

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    req.once('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      name: 'Error',
      message: 'Stream closed with error code NGHTTP2_INTERNAL_ERROR'
    }));
    req.on('response', common.mustCall());
    req.once('data', common.mustCall(() => {
      net.Socket.prototype.destroy.call(client.socket);
      server.close();
    }));
    req.once('end', common.mustNotCall());
    req.end();
  }));
})().then(common.mustCall());
