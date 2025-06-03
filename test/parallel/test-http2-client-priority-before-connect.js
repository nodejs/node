'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

common.expectWarning(
  'DeprecationWarning',
  'http2Stream.priority is longer supported after priority signalling was deprecated in RFC 1993',
  'DEP0194');

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.priority({});

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
