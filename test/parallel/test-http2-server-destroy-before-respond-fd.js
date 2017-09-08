// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Test that ERR_HTTP2_INVALID_STREAM is thrown when the stream session
// is destroyed

const server = http2.createServer();

server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.session.destroy();

  common.expectsError(
    () => stream.respondWithFD(0), {
      code: 'ERR_HTTP2_INVALID_STREAM',
      message: /^The stream has been destroyed$/
    });
}

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request({ ':path': '/' });

  req.on('response', common.mustNotCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();
}));
