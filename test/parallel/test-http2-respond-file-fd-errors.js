'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');
const { inspect } = require('util');

const optionsWithTypeError = {
  offset: 'number',
  length: 'number',
  statCheck: 'function'
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
const fd = fs.openSync(fname, 'r');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  // Should throw if fd isn't a number
  Object.keys(types).forEach((type) => {
    if (type === 'number') {
      return;
    }

    assert.throws(
      () => stream.respondWithFD(types[type], {
        'content-type': 'text/plain'
      }),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "fd" argument must be of type number or an instance of' +
                 ` FileHandle.${common.invalidArgTypeHelper(types[type])}`
      }
    );
  });

  // Check for all possible TypeError triggers on options
  Object.keys(optionsWithTypeError).forEach((option) => {
    Object.keys(types).forEach((type) => {
      if (type === optionsWithTypeError[option]) {
        return;
      }

      assert.throws(
        () => stream.respondWithFD(fd, {
          'content-type': 'text/plain'
        }, {
          [option]: types[type]
        }),
        {
          name: 'TypeError',
          code: 'ERR_INVALID_ARG_VALUE',
          message: `The property 'options.${option}' is invalid. ` +
            `Received ${inspect(types[type])}`
        }
      );
    });
  });

  // Should throw if :status 204, 205 or 304
  [204, 205, 304].forEach((status) => assert.throws(
    () => stream.respondWithFD(fd, {
      'content-type': 'text/plain',
      ':status': status,
    }),
    {
      code: 'ERR_HTTP2_PAYLOAD_FORBIDDEN',
      name: 'Error',
      message: `Responses with ${status} status must not have a payload`
    }
  ));

  // Should throw if headers already sent
  stream.respond();
  assert.throws(
    () => stream.respondWithFD(fd, {
      'content-type': 'text/plain'
    }),
    {
      code: 'ERR_HTTP2_HEADERS_SENT',
      name: 'Error',
      message: 'Response has already been initiated.'
    }
  );

  // Should throw if stream already destroyed
  stream.destroy();
  assert.throws(
    () => stream.respondWithFD(fd, {
      'content-type': 'text/plain'
    }),
    {
      code: 'ERR_HTTP2_INVALID_STREAM',
      name: 'Error',
      message: 'The stream has been destroyed'
    }
  );
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
}));
