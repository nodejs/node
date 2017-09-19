// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

// Test that ERR_HTTP2_INVALID_STREAM is thrown when a stream is destroyed
// before calling stream.rstStream
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.session.destroy();
  assert.throws(() => stream.rstStream(),
                common.expectsError({
                  code: 'ERR_HTTP2_INVALID_STREAM',
                  message: /^The stream has been destroyed$/
                }));
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':path': '/' });

  req.on('response', common.mustNotCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();

}));
