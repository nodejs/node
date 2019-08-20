'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const http2 = require('http2');

// This test will result in a crash due to a missed CHECK in C++ or
// a straight-up segfault if the C++ doesn't send RST_STREAM through
// properly when calling destroy.

const content = Buffer.alloc(60000, 0x44);

const server = http2.createSecureServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
});
server.on('stream', common.mustCall((stream) => {
  stream.respond({
    'Content-Type': 'application/octet-stream',
    'Content-Length': (content.length.toString() * 2),
    'Vary': 'Accept-Encoding'
  }, { waitForTrailers: true });

  stream.write(content);
  stream.destroy();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`https://localhost:${server.address().port}`,
                               { rejectUnauthorized: false });

  const req = client.request({ ':path': '/' });
  req.end();
  req.resume(); // Otherwise close won't be emitted if there's pending data.

  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));
}));
