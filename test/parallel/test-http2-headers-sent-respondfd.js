// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Test that ERR_HTTP2_HEADERS_SENT error is thrown when headers are sent before
// calling ServerHttp2Stream.respondWithFD

const server = http2.createServer();

const {
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;

server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });

  common.expectsError(() => stream.respondWithFD(0, {
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  }), {
    type: Error,
    code: 'ERR_HTTP2_HEADERS_SENT',
    message: 'Response has already been initiated.'
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
