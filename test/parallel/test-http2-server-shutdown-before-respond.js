'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.session.goaway(1);
  stream.respond();
  stream.end('data');
}

server.listen(0);

server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);

  client.on('goaway', common.mustCall());
  client.on('error', common.expectsError({
    code: 'ERR_HTTP2_SESSION_ERROR'
  }));

  const req = client.request();
  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_SESSION_ERROR'
  }));
  req.resume();
  req.on('data', common.mustNotCall());
  req.on('end', common.mustNotCall());
  req.on('close', common.mustCall(() => server.close()));
}));
