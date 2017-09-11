// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const path = require('path');
const fs = require('fs');

const optionsWithTypeError = {
  offset: 'number',
  length: 'number',
  statCheck: 'function',
  getTrailers: 'function'
};

const types = {
  boolean: true,
  function: () => {},
  number: 1,
  object: {},
  array: [],
  null: null,
  symbol: Symbol('test')
};

const fname = path.resolve(common.fixturesDir, 'elipses.txt');
const fd = fs.openSync(fname, 'r');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  // should throw if fd isn't a number
  Object.keys(types).forEach((type) => {
    if (type === 'number') {
      return;
    }

    common.expectsError(
      () => stream.respondWithFD(types[type], {
        [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
      }),
      {
        type: TypeError,
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "fd" argument must be of type number'
      }
    );
  });

  // Check for all possible TypeError triggers on options
  Object.keys(optionsWithTypeError).forEach((option) => {
    Object.keys(types).forEach((type) => {
      if (type === optionsWithTypeError[option]) {
        return;
      }

      common.expectsError(
        () => stream.respondWithFD(fd, {
          [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
        }, {
          [option]: types[type]
        }),
        {
          type: TypeError,
          code: 'ERR_INVALID_OPT_VALUE',
          message: `The value "${String(types[type])}" is invalid ` +
                   `for option "${option}"`
        }
      );
    });
  });

  // Should throw if :status 204, 205 or 304
  [204, 205, 304].forEach((status) => common.expectsError(
    () => stream.respondWithFD(fd, {
      [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/plain',
      ':status': status,
    }),
    {
      code: 'ERR_HTTP2_PAYLOAD_FORBIDDEN',
      message: `Responses with ${status} status must not have a payload`
    }
  ));

  // Should throw if headers already sent
  stream.respond({
    ':status': 200,
  });
  common.expectsError(
    () => stream.respondWithFD(fd, {
      [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
    }),
    {
      code: 'ERR_HTTP2_HEADERS_SENT',
      message: 'Response has already been initiated.'
    }
  );

  // Should throw if stream already destroyed
  stream.destroy();
  common.expectsError(
    () => stream.respondWithFD(fd, {
      [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
    }),
    {
      code: 'ERR_HTTP2_INVALID_STREAM',
      message: 'The stream has been destroyed'
    }
  );
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('streamClosed', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
  req.end();
}));
