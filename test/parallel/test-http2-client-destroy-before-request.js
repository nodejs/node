// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustNotCall());

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);
  client.destroy();

  assert.throws(() => client.request({ ':path': '/' }),
                common.expectsError({
                  code: 'ERR_HTTP2_INVALID_SESSION',
                  message: /^The session has been destroyed$/
                }));

  server.close();

}));
