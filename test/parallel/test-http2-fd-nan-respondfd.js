// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Test that ERR_INVALID_ARG_TYPE is thrown when ServerHttp2Stream.respondWithFD
// is called with an fd value that is not of type number

const types = {
  function: () => {},
  object: {},
  array: [],
  null: null,
  symbol: Symbol('test')
};

const server = http2.createServer();

server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  Object.keys(types).forEach((type) => {

    common.expectsError(() => stream.respondWithFD(type), {
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "fd" argument must be of type number'
    });

  });
  stream.end('hello world');
}

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);
  const req = client.request();

  req.resume();

  req.on('end', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
  req.end();
}));
