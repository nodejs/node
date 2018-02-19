// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const { Http2Stream } = process.binding('http2');

const types = {
  boolean: true,
  function: () => {},
  number: 1,
  object: {},
  array: [],
  null: null,
  symbol: Symbol('test')
};

const server = http2.createServer();

Http2Stream.prototype.respond = () => 1;
server.on('stream', common.mustCall((stream) => {

  // Check for all possible TypeError triggers on options.getTrailers
  Object.entries(types).forEach(([type, value]) => {
    if (type === 'function') {
      return;
    }

    common.expectsError(
      () => stream.respond({
        'content-type': 'text/plain'
      }, {
        ['getTrailers']: value
      }),
      {
        type: TypeError,
        code: 'ERR_INVALID_OPT_VALUE',
        message: `The value "${String(value)}" is invalid ` +
                  'for option "getTrailers"'
      }
    );
  });

  // Send headers
  stream.respond({
    'content-type': 'text/plain'
  }, {
    ['getTrailers']: () => common.mustCall()
  });

  // Should throw if headers already sent
  common.expectsError(
    () => stream.respond(),
    {
      type: Error,
      code: 'ERR_HTTP2_HEADERS_SENT',
      message: 'Response has already been initiated.'
    }
  );

  // Should throw if stream already destroyed
  stream.destroy();
  common.expectsError(
    () => stream.respond(),
    {
      type: Error,
      code: 'ERR_HTTP2_INVALID_STREAM',
      message: 'The stream has been destroyed'
    }
  );
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.resume();
  req.end();
}));
