'use strict';
// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const { internalBinding } = require('internal/test/binding');
const { Http2Stream } = internalBinding('http2');

const server = http2.createServer();

Http2Stream.prototype.respond = () => 1;
server.on('stream', common.mustCall((stream) => {

  // Send headers
  stream.respond({ 'content-type': 'text/plain' });

  // Should throw if headers already sent
  common.expectsError(
    () => stream.respond(),
    {
      type: Error,
      code: 'ERR_HTTP2_HEADERS_SENT',
      message: 'Response has already been initiated.'
    }
  );

  // Should throw if stream already destroyed
  stream.destroy();
  common.expectsError(
    () => stream.respond(),
    {
      type: Error,
      code: 'ERR_HTTP2_INVALID_STREAM',
      message: 'The stream has been destroyed'
    }
  );
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.resume();
  req.end();
}));
