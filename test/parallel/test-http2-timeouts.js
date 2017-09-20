// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustCall((stream) => {
  stream.setTimeout(1, common.mustCall(() => {
    stream.respond({ ':status': 200 });
    stream.end('hello world');
  }));
}));
server.listen(0);

server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  client.setTimeout(1, common.mustCall(() => {
    const req = client.request({ ':path': '/' });
    req.setTimeout(1, common.mustCall(() => {
      req.on('response', common.mustCall());
      req.resume();
      req.on('end', common.mustCall(() => {
        server.close();
        client.destroy();
      }));
      req.end();
    }));
  }));
}));
