'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();
server.setTimeout(common.platformTimeout(1));

const onServerTimeout = common.mustCall((session) => {
  session.close(() => session.destroy());
});

server.on('stream', common.mustNotCall());
server.once('timeout', onServerTimeout);

server.listen(0, common.mustCall(() => {
  const url = `http://localhost:${server.address().port}`;
  const client = http2.connect(url);
  // Because of the timeout, an ECONNRESET error may or may not happen here.
  // Keep this as a non-op and do not use common.mustCall()
  client.on('error', () => {});
  client.on('close', common.mustCall(() => {
    const client2 = http2.connect(url);
    // Because of the timeout, an ECONNRESET error may or may not happen here.
    // Keep this as a non-op and do not use common.mustCall()
    client2.on('error', () => {});
    client2.on('close', common.mustCall(() => server.close()));
  }));
}));
