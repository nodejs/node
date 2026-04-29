'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

const types = [
  true,
  {},
  [],
  null,
  new Date(),
];

server.on('stream', common.mustCall((stream) => {
  const session = stream.session;

  for (const input of types) {
    const received = common.invalidArgTypeHelper(input);
    assert.throws(
      () => session.goaway(input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "code" argument must be of type number.' +
                 received
      }
    );
    assert.throws(
      () => session.goaway(0, input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "lastStreamID" argument must be of type number.' +
                 received
      }
    );
    assert.throws(
      () => session.goaway(0, 0, input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "opaqueData" argument must be an instance of Buffer, ' +
                 `TypedArray, or DataView.${received}`
      }
    );
  }

  stream.session.destroy();
}));

server.listen(
  0,
  common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    // The validation logic is exercised on the server side. The client stream
    // may close cleanly or with an error depending on platform-specific
    // teardown ordering.
    req.on('error', () => {});
    req.resume();
    client.on('close', common.mustCall(() => {
      server.close();
    }));
  })
);
