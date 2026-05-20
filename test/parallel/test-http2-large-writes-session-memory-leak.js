'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const http2 = require('http2');

// Regression test for https://github.com/nodejs/node/issues/29223.
// There was a "leak" in the accounting of session memory leading
// to streams eventually failing with NGHTTP2_ENHANCE_YOUR_CALM.

const server = http2.createSecureServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
});

// Simple server that sends 200k and closes the stream.
const data200k = 'a'.repeat(200 * 1024);
server.on('stream', (stream) => {
  stream.write(data200k);
  stream.end();
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`https://localhost:${server.address().port}`, {
    ca: fixtures.readKey('agent2-cert.pem'),
    servername: 'agent2',

    // Set maxSessionMemory to 1MB so the leak causes errors faster.
    maxSessionMemory: 1
  });

  // Repeatedly create a new stream and read the incoming data. Even though we
  // only have one stream active at a time, prior to the fix for #29223,
  // session memory would steadily increase and we'd eventually hit the 1MB
  // maxSessionMemory limit and get NGHTTP2_ENHANCE_YOUR_CALM errors trying to
  // create new streams.
  let streamsLeft = 50;
  function newStream() {
    const stream = client.request({ ':path': '/' });

    stream.on('data', () => { });

    stream.on('close', () => {
      if (streamsLeft-- > 0) {
        newStream();
      } else {
        client.destroy();
        server.close();
      }
    });
  }

  newStream();
}));
