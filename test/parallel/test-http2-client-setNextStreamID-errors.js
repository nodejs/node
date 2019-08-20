'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond();
  stream.end('ok');
});

const types = {
  boolean: true,
  function: () => {},
  number: 1,
  object: {},
  array: [],
  null: null,
  symbol: Symbol('test')
};

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  client.on('connect', () => {
    const outOfRangeNum = 2 ** 31;
    common.expectsError(
      () => client.setNextStreamID(outOfRangeNum),
      {
        type: RangeError,
        code: 'ERR_OUT_OF_RANGE',
        message: 'The value of "id" is out of range.' +
           ' It must be > 0 and <= 2147483647. Received ' + outOfRangeNum
      }
    );

    // Should throw if something other than number is passed to setNextStreamID
    Object.entries(types).forEach(([type, value]) => {
      if (type === 'number') {
        return;
      }

      common.expectsError(
        () => client.setNextStreamID(value),
        {
          type: TypeError,
          code: 'ERR_INVALID_ARG_TYPE',
          message: 'The "id" argument must be of type number. Received type ' +
                   typeof value
        }
      );
    });

    server.close();
    client.close();
  });
}));
