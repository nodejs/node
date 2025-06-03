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
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.priority({
    parent: 0,
    weight: 1,
    exclusive: false
  });
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('hello world');
}

server.listen(0);

server.on('priority', common.mustNotCall());

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':path': '/' });

  client.on('connect', () => {
    req.priority({
      parent: 0,
      weight: 1,
      exclusive: false
    });
  });

  // The priority event is not supported anymore by nghttp2
  // since 1.65.0.
  req.on('priority', common.mustNotCall());

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.close();
  }));
  req.end();

}));
