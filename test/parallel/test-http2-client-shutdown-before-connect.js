'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  client.close(common.mustCall(() => server.close()));
}));
