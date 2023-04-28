'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const http2 = require('http2');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');

const server = http2.createSecureServer({ key, cert });

let client_stream;

server.on('stream', common.mustCall(function(stream) {
  stream.resume();
  stream.on('data', function(chunk) {
    stream.write(chunk);
    client_stream.pause();
    client_stream.close(http2.constants.NGHTTP2_CANCEL);
  });
  stream.on('error', () => {});
}));

server.listen(0, function() {
  const client = http2.connect(`https://localhost:${server.address().port}`,
                               { rejectUnauthorized: false }
  );
  client_stream = client.request({ ':method': 'POST' });
  client_stream.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));
  client_stream.resume();
  client_stream.write(Buffer.alloc(1024 * 1024));
});
