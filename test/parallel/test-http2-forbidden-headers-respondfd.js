// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Test that ERR_HTTP2_PAYLOAD_FORBIDDEN is thrown when 
// ServerHttp2Stream.respondWithFD is called with a forbidden header status

const server = http2.createServer();

const {
  HTTP2_HEADER_STATUS
} = http2.constants;

const {
  constants
} = process.binding('http2');

const statuses = [
  constants.HTTP_STATUS_NO_CONTENT,
  constants.HTTP_STATUS_RESET_CONTENT,
  constants.HTTP_STATUS_NOT_MODIFIED
];

server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  statuses.forEach((headerStatus) => {

    common.expectsError(() => stream.respondWithFD(0, {
      [HTTP2_HEADER_STATUS]: headerStatus
    }), {
      type: Error,
      code: 'ERR_HTTP2_PAYLOAD_FORBIDDEN',
      message: `Responses with ${headerStatus} status must not have a payload`
    });

  });
  stream.end('hello world');
}

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request();

  req.resume();

  req.on('end', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
  req.end();
}));
