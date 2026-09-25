// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');
const { kSocket } = require('internal/http2/util');

const body =
  '<html><head></head><body><h1>this is some data</h2></body></html>';

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall((stream) => {
  stream.on('error', () => {});
  stream.on('aborted', common.mustCall());
  stream.on('close', common.mustCall());
  stream.respond();
  stream.write(body);
  // Purposefully do not end()
}));

server.listen(0, common.mustCall(function() {
  const client = h2.connect(`http://localhost:${this.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall(() => {
    // Send a premature socket close
    client[kSocket].destroy();
  }));

  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => server.close()));

  // On the client, the close event must call
  client.on('close', common.mustCall());
}));
