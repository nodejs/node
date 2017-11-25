'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const http2 = require('http2');

const {
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_METHOD
} = http2.constants;

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

const fname = fixtures.path('elipses.txt');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  // Check for all possible TypeError triggers on options
  Object.keys(optionsWithTypeError).forEach((option) => {
    Object.keys(types).forEach((type) => {
      if (type === optionsWithTypeError[option]) {
        return;
      }

      common.expectsError(
        () => stream.respondWithFile(fname, {
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
    () => stream.respondWithFile(fname, {
      [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain',
      ':status': status,
    }),
    {
      code: 'ERR_HTTP2_PAYLOAD_FORBIDDEN',
      message: `Responses with ${status} status must not have a payload`
    }
  ));

  // should emit an error on the stream if headers aren't valid
  stream.respondWithFile(fname, {
    [HTTP2_HEADER_METHOD]: 'POST'
  }, {
    statCheck: common.mustCall(() => {
      // give time to the current test case to finish
      process.nextTick(continueTest, stream);
      return true;
    })
  });
  stream.once('error', common.expectsError({
    code: 'ERR_HTTP2_INVALID_PSEUDOHEADER',
    type: Error,
    message: '":method" is an invalid pseudoheader or is used incorrectly'
  }));
}));

function continueTest(stream) {
  // Should throw if headers already sent
  stream.respond({
    ':status': 200,
  });
  common.expectsError(
    () => stream.respondWithFile(fname, {
      [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
    }),
    {
      code: 'ERR_HTTP2_HEADERS_SENT',
      message: 'Response has already been initiated.'
    }
  );

  // Should throw if stream already destroyed
  stream.destroy();
  common.expectsError(
    () => stream.respondWithFile(fname, {
      [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
    }),
    {
      code: 'ERR_HTTP2_INVALID_STREAM',
      message: 'The stream has been destroyed'
    }
  );
}

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('close', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
  req.end();
}));
