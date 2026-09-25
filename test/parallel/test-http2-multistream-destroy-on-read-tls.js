'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const http2 = require('http2');

// Regression test for https://github.com/nodejs/node/issues/29353.
// Test that it’s okay for an HTTP2 + TLS server to destroy a stream instance
// while reading it.

const server = http2.createSecureServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
});

const filenames = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'];

server.on('stream', common.mustCall((stream) => {
  function write() {
    stream.write('a'.repeat(10240));
    stream.once('drain', write);
  }
  write();
}, filenames.length));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`https://localhost:${server.address().port}`, {
    ca: fixtures.readKey('agent2-cert.pem'),
    servername: 'agent2'
  });

  let destroyed = 0;
  for (const entry of filenames) {
    const stream = client.request({
      ':path': `/${entry}`
    });
    stream.once('data', common.mustCall(() => {
      stream.destroy();

      if (++destroyed === filenames.length) {
        client.destroy();
        server.close();
      }
    }));
  }
}));
