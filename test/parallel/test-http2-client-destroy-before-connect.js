// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustNotCall());

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':path': '/' });
  client.destroy();

  req.on('response', common.mustNotCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
  }));
  req.end();

}));
