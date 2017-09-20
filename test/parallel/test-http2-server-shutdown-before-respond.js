// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  const session = stream.session;
  stream.session.shutdown({ graceful: true }, common.mustCall(() => {
    session.destroy();
  }));
  stream.respond({});
  stream.end('data');
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  client.on('goaway', common.mustCall());

  const req = client.request();

  req.resume();
  req.on('end', common.mustCall(() => server.close()));
}));
