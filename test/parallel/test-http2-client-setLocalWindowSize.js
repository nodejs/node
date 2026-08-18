'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

{
  const server = http2.createServer();
  server.on('stream', common.mustNotCall((stream) => {
    stream.respond();
    stream.end('ok');
  }));

  const types = {
    boolean: true,
    function: () => {},
    number: 1,
    object: {},
    array: [],
    null: null,
  };

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);

    client.on('connect', common.mustCall(() => {
      const outOfRangeNum = 2 ** 32;
      assert.throws(
        () => client.setLocalWindowSize(outOfRangeNum),
        {
          name: 'RangeError',
          code: 'ERR_OUT_OF_RANGE',
          message: 'The value of "windowSize" is out of range.' +
            ' It must be >= 0 && <= 2147483647. Received ' + outOfRangeNum
        }
      );

      // Throw if something other than number is passed to setLocalWindowSize
      Object.entries(types).forEach(([type, value]) => {
        if (type === 'number') {
          return;
        }

        assert.throws(
          () => client.setLocalWindowSize(value),
          {
            name: 'TypeError',
            code: 'ERR_INVALID_ARG_TYPE',
            message: 'The "windowSize" argument must be of type number.' +
                    common.invalidArgTypeHelper(value)
          }
        );
      });

      server.close();
      client.close();
    }));
  }));
}

{
  const server = http2.createServer();
  server.on('stream', common.mustNotCall((stream) => {
    stream.respond();
    stream.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);

    client.on('connect', common.mustCall(() => {
      const windowSize = 2 ** 20;
      client.setLocalWindowSize(windowSize);

      assert.strictEqual(client.state.effectiveLocalWindowSize, windowSize);
      // localWindowSize returns the available connection window.
      // When decreasing from the default 33554432 to 1048576,
      // the available window stays at 33554432.
      assert.strictEqual(client.state.localWindowSize, 33554432);
      // remoteWindowSize is the connection-level send window,
      // which remains at the HTTP/2 default of 65535.
      assert.strictEqual(client.state.remoteWindowSize, 65535);

      server.close();
      client.close();
    }));
  }));
}

{
  const server = http2.createServer();
  server.on('stream', common.mustNotCall((stream) => {
    stream.respond();
    stream.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);

    client.on('connect', common.mustCall(() => {
      const windowSize = 20;
      client.setLocalWindowSize(windowSize);

      assert.strictEqual(client.state.effectiveLocalWindowSize, windowSize);
      // localWindowSize returns the available connection window.
      // When decreasing from the default 33554432 to 20,
      // the available window stays at 33554432.
      assert.strictEqual(client.state.localWindowSize, 33554432);
      // remoteWindowSize is the connection-level send window,
      // which remains at the HTTP/2 default of 65535.
      assert.strictEqual(client.state.remoteWindowSize, 65535);

      server.close();
      client.close();
    }));
  }));
}
