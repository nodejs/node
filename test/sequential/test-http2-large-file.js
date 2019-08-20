'use strict';

// Test sending a large stream with a large initial window size.
// See: https://github.com/nodejs/node/issues/19141

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const server = http2.createServer({ settings: { initialWindowSize: 6553500 } });
server.on('stream', (stream) => {
  stream.resume();
  stream.respond();
  stream.end('ok');
});

server.listen(0, common.mustCall(() => {
  let remaining = 1e8;
  const chunkLength = 1e6;
  const chunk = Buffer.alloc(chunkLength, 'a');
  const client = http2.connect(`http://localhost:${server.address().port}`,
                               { settings: { initialWindowSize: 6553500 } });
  const request = client.request({ ':method': 'POST' });
  function writeChunk() {
    if (remaining > 0) {
      remaining -= chunkLength;
      request.write(chunk, writeChunk);
    } else {
      request.end();
    }
  }
  writeChunk();
  request.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));
  request.resume();
}));
