// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const { kSocket } = require('internal/http2/util');

{
  const server = h2.createServer();
  server.listen(
    0,
    common.mustCall(() => {
      const destroyCallbacks = [
        (client) => client.destroy(),
        (client) => client[kSocket].destroy()
      ];

      let remaining = destroyCallbacks.length;

      destroyCallbacks.forEach((destroyCallback) => {
        const client = h2.connect(`http://localhost:${server.address().port}`);
        client.on(
          'connect',
          common.mustCall(() => {
            const socket = client[kSocket];

            assert(socket, 'client session has associated socket');
            assert(
              !client.destroyed,
              'client has not been destroyed before destroy is called'
            );
            assert(
              !socket.destroyed,
              'socket has not been destroyed before destroy is called'
            );

            // Ensure that 'close' event is emitted
            client.on('close', common.mustCall());

            destroyCallback(client);

            assert(
              !client[kSocket],
              'client.socket undefined after destroy is called'
            );

            // Must must be closed
            client.on(
              'close',
              common.mustCall(() => {
                assert(client.destroyed);
              })
            );

            // socket will close on process.nextTick
            socket.on(
              'close',
              common.mustCall(() => {
                assert(socket.destroyed);
              })
            );

            if (--remaining === 0) {
              server.close();
            }
          })
        );
      });
    })
  );
}

// test destroy before client operations
{
  const server = h2.createServer();
  server.listen(
    0,
    common.mustCall(() => {
      const client = h2.connect(`http://localhost:${server.address().port}`);
      const req = client.request();
      client.destroy();

      req.on('response', common.mustNotCall());
      req.resume();

      const sessionError = {
        type: Error,
        code: 'ERR_HTTP2_INVALID_SESSION',
        message: 'The session has been destroyed'
      };

      common.expectsError(() => client.request(), sessionError);
      common.expectsError(() => client.settings({}), sessionError);
      common.expectsError(() => client.priority(req, {}), sessionError);
      common.expectsError(() => client.shutdown(), sessionError);

      // Wait for setImmediate call from destroy() to complete
      // so that state.destroyed is set to true
      setImmediate(() => {
        common.expectsError(() => client.request(), sessionError);
        common.expectsError(() => client.settings({}), sessionError);
        common.expectsError(() => client.priority(req, {}), sessionError);
        common.expectsError(() => client.shutdown(), sessionError);
        common.expectsError(() => client.rstStream(req), sessionError);
      });

      req.on(
        'end',
        common.mustCall(() => {
          server.close();
        })
      );
      req.end();
    })
  );
}

// test destroy before goaway
{
  const server = h2.createServer();
  server.on(
    'stream',
    common.mustCall((stream) => {
      stream.on('error', common.mustCall());
      stream.session.shutdown();
    })
  );
  server.listen(
    0,
    common.mustCall(() => {
      const client = h2.connect(`http://localhost:${server.address().port}`);

      client.on(
        'goaway',
        common.mustCall(() => {
          // We ought to be able to destroy the client in here without an error
          server.close();
          client.destroy();
        })
      );

      client.request();
    })
  );
}
