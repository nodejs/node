'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

server.on(
  'stream',
  common.mustCall((stream) => {
    const invalidArgTypeError = (param, type) => ({
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE',
      message: `The "${param}" argument must be of type ${type}`
    });
    common.expectsError(
      () => stream.rstStream('string'),
      invalidArgTypeError('code', 'number')
    );
    stream.session.destroy();
  })
);

server.listen(
  0,
  common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.resume();
    req.on('end', common.mustCall(() => server.close()));
    req.end();
  })
);
