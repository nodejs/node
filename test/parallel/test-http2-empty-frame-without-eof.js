'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { readSync } = require('../common/fixtures');
const net = require('net');
const http2 = require('http2');
const { once } = require('events');

async function main() {
  const blobWithEmptyFrame = readSync('emptyframe.http2');
  const server = net.createServer((socket) => {
    socket.once('data', () => {
      socket.end(blobWithEmptyFrame);
    });
  }).listen(0);
  await once(server, 'listening');

  for (const maxSessionInvalidFrames of [0, 2]) {
    const client = http2.connect(`http://localhost:${server.address().port}`, {
      maxSessionInvalidFrames
    });
    const stream = client.request({
      ':method': 'GET',
      ':path': '/'
    });
    if (maxSessionInvalidFrames) {
      stream.on('error', common.mustNotCall());
      client.on('error', common.mustNotCall());
    } else {
      const expected = {
        code: 'ERR_HTTP2_TOO_MANY_INVALID_FRAMES',
        message: 'Too many invalid HTTP/2 frames'
      };
      stream.on('error', common.expectsError(expected));
      client.on('error', common.expectsError(expected));
    }
    stream.resume();
    await new Promise((resolve) => {
      stream.once('close', resolve);
    });
    client.close();
  }
  server.close();
}

main().then(common.mustCall());
