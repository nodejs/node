'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

server.on('stream', common.mustNotCall());
server.on('error', common.mustNotCall());

server.listen(0, common.mustCall(() => {

  // Setting the maxSendHeaderBlockLength > nghttp2 threshold
  // cause a 'sessionError' and no memory leak when session destroy
  const options = {
    maxSendHeaderBlockLength: 100000
  };

  const client = h2.connect(`http://localhost:${server.address().port}`,
                            options);
  client.on('error', common.expectsError({
    code: 'ERR_HTTP2_SESSION_ERROR',
    name: 'Error',
    message: 'Session closed with error code 9'
  }));

  const req = client.request({
    // Greater than 65536 bytes
    'test-header': 'A'.repeat(90000)
  });
  req.on('response', common.mustNotCall());

  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));

  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_SESSION_ERROR',
    name: 'Error',
    message: 'Session closed with error code 9'
  }));
  req.end();
}));
