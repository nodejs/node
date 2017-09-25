'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

// Test that ERR_HTTP2_INVALID_SESSION is thrown when a stream is destroyed
// before calling stream.session.shutdown
server.on('stream', common.mustCall((stream) => {
  stream.session.destroy();
  common.expectsError(
    () => stream.session.shutdown(),
    {
      type: Error,
      code: 'ERR_HTTP2_INVALID_SESSION',
      message: 'The session has been destroyed'
    }
  );
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.resume();
  req.on('end', common.mustCall(() => server.close()));
}));
