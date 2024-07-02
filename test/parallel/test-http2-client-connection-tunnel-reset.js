// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');
const { kSocket } = require('internal/http2/util');

const h2Server = h2.createServer(() => {});

h2Server.on('connect', (req, res) => {
  res.writeHead(200, {});

  res.stream.session[kSocket].resetAndDestroy();

  h2Server.close();
});

h2Server.listen(0, common.mustCall(() => {
  const proxyClient = h2.connect(`http://localhost:${h2Server.address().port}`);
  proxyClient.once('error', common.expectsError({
    name: 'Error',
    code: 'ECONNRESET',
    message: 'read ECONNRESET'
  }));
  const proxyReq = proxyClient.request({
    ':method': 'CONNECT',
    ':authority': 'example.com:443'
  });
  proxyReq.once('error', common.expectsError({
    name: 'Error',
    code: 'ECONNRESET',
    message: 'read ECONNRESET'
  }));
}));
