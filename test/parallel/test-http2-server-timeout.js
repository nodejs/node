'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

function testServerTimeout(setTimeoutFn) {
  const server = http2.createServer();
  setTimeoutFn(server);

  const onServerTimeout = common.mustCall((session) => {
    session.close();
  });

  server.on('stream', common.mustNotCall());
  server.once('timeout', onServerTimeout);

  server.listen(0, common.mustCall(() => {
    const url = `http://localhost:${server.address().port}`;
    const client = http2.connect(url);
    client.on('close', common.mustCall(() => {
      const client2 = http2.connect(url);
      client2.on('close', common.mustCall(() => server.close()));
    }));
  }));
}

const timeout = common.platformTimeout(50);
testServerTimeout((server) => server.setTimeout(timeout));
testServerTimeout((server) => server.timeout = timeout);
