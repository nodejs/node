'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.respond(undefined, { waitForTrailers: true });
  // There is no wantTrailers handler so this should close naturally
  // without hanging. If the test completes without timing out, then
  // it passes.
  stream.end('ok');
}

server.listen(0);

server.on('listening', common.mustCall(function() {
  const client = h2.connect(`http://localhost:${this.address().port}`);
  const req = client.request();
  req.resume();
  req.on('trailers', common.mustNotCall());
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
