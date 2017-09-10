// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Test that ERR_INVALID_OPT_VALUE TypeErrors are thrown when passing invalid
// options to ServerHttp2Stream.respondWithFD method

const optionsToTest = {
  offset: 'number',
  length: 'number',
  statCheck: 'function',
  getTrailers: 'function'
};

const types = {
  function: () => {},
  number: 1,
  object: {},
  array: [],
  null: null,
  symbol: Symbol('test')
};

const server = http2.createServer();

const {
  HTTP2_HEADER_CONTENT_TYPE
} = http2.constants;

server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  Object.keys(optionsToTest).forEach((option) => {
    Object.keys(types).forEach((type) => {
      if (type === optionsToTest[option]) {
        return;
      }

      common.expectsError(() => stream.respondWithFD(0, {
        [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
      }, {
        [option]: types[type]
      }), {
        type: TypeError,
        code: 'ERR_INVALID_OPT_VALUE',
        message: `The value "${String(types[type])}" is invalid ` +
          `for option "${option}"`
      });

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
