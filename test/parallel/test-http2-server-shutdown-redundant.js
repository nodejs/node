// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

// Test blank return when a stream.session.shutdown is called twice
// Also tests stream.session.shutdown with just a callback function (no options)
server.on('stream', common.mustCall((stream) => {
  stream.session.shutdown(common.mustCall(() => {
    assert.strictEqual(
      stream.session.shutdown(common.mustNotCall()),
      undefined
    );
  }));
  stream.session.shutdown(common.mustNotCall());
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.resume();
  req.on('end', common.mustCall(() => server.close()));
}));
