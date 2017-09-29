// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

const optionsToTest = {
  opaqueData: 'Uint8Array',
  graceful: 'boolean',
  errorCode: 'number',
  lastStreamID: 'number'
};

const types = {
  boolean: true,
  number: 1,
  object: {},
  array: [],
  null: null,
  Uint8Array: Buffer.from([0x1, 0x2, 0x3, 0x4, 0x5])
};

server.on(
  'stream',
  common.mustCall((stream) => {
    Object.keys(optionsToTest).forEach((option) => {
      Object.keys(types).forEach((type) => {
        if (type === optionsToTest[option]) {
          return;
        }
        common.expectsError(
          () =>
            stream.session.shutdown(
              { [option]: types[type] },
              common.mustNotCall()
            ),
          {
            type: TypeError,
            code: 'ERR_INVALID_OPT_VALUE',
            message: `The value "${String(types[type])}" is invalid ` +
            `for option "${option}"`
          }
        );
      });
    });
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
  })
);
