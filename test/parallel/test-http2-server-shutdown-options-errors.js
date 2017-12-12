'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

const types = [
  true,
  {},
  [],
  null,
  new Date()
];

server.on('stream', common.mustCall((stream) => {
  const session = stream.session;

  types.forEach((i) => {
    common.expectsError(
      () => session.goaway(i),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "code" argument must be of type number'
      }
    );
    common.expectsError(
      () => session.goaway(0, i),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "lastStreamID" argument must be of type number'
      }
    );
    common.expectsError(
      () => session.goaway(0, 0, i),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "opaqueData" argument must be one of type Buffer, ' +
                 'TypedArray, or DataView'
      }
    );
  });

  stream.session.destroy();
}));

server.listen(
  0,
  common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    // On certain operating systems, an ECONNRESET may occur. We do not need
    // to test for it here. Do not make this a mustCall
    client.on('error', () => {});
    const req = client.request();
    // On certain operating systems, an ECONNRESET may occur. We do not need
    // to test for it here. Do not make this a mustCall
    req.on('error', () => {});
    req.resume();
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  })
);
