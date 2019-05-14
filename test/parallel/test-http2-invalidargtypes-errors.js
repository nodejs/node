'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  common.expectsError(
    () => stream.close('string'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "code" argument must be of type number. ' +
               'Received type string'
    }
  );
  stream.respond();
  stream.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.resume();
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
