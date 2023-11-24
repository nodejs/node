// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const { kSocket } = require('internal/http2/util');
const Countdown = require('../common/countdown');
const { getEventListeners } = require('events');
{
  const server = h2.createServer();
  server.listen(0, common.mustCall(() => {
    const destroyCallbacks = [
      (client) => client.destroy(),
      (client) => client[kSocket].destroy(),
    ];

    const countdown = new Countdown(destroyCallbacks.length, () => {
      server.close();
    });

    for (const destroyCallback of destroyCallbacks) {
      const client = h2.connect(`http://localhost:${server.address().port}`);
      client.on('connect', common.mustCall(() => {
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

        destroyCallback(client);

        client.on('close', common.mustCall(() => {
          assert(client.destroyed);
        }));

        countdown.dec();
      }));
    }
  }));
}

// Test destroy before client operations
{
  const server = h2.createServer();
  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    const socket = client[kSocket];
    socket.on('close', common.mustCall(() => {
      assert(socket.destroyed);
    }));

    const req = client.request();
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_CANCEL',
      name: 'Error',
      message: 'The pending stream has been canceled'
    }));

    client.destroy();

    req.on('response', common.mustNotCall());

    const sessionError = {
      name: 'Error',
      code: 'ERR_HTTP2_INVALID_SESSION',
      message: 'The session has been destroyed'
    };

    assert.throws(() => client.setNextStreamID(), sessionError);
    assert.throws(() => client.setLocalWindowSize(), sessionError);
    assert.throws(() => client.ping(), sessionError);
    assert.throws(() => client.settings({}), sessionError);
    assert.throws(() => client.goaway(), sessionError);
    assert.throws(() => client.request(), sessionError);
    client.close();  // Should be a non-op at this point

    // Wait for setImmediate call from destroy() to complete
    // so that state.destroyed is set to true
    setImmediate(() => {
      assert.throws(() => client.setNextStreamID(), sessionError);
      assert.throws(() => client.setLocalWindowSize(), sessionError);
      assert.throws(() => client.ping(), sessionError);
      assert.throws(() => client.settings({}), sessionError);
      assert.throws(() => client.goaway(), sessionError);
      assert.throws(() => client.request(), sessionError);
      client.close();  // Should be a non-op at this point
    });

    req.resume();
    req.on('end', common.mustNotCall());
    req.on('close', common.mustCall(() => server.close()));
  }));
}

// Test destroy before goaway
{
  const server = h2.createServer();
  server.on('stream', common.mustCall((stream) => {
    stream.session.destroy();
  }));

  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);

    client.on('close', () => {
      server.close();
      // Calling destroy in here should not matter
      client.destroy();
    });

    client.request();
  }));
}

// Test destroy before connect
{
  const server = h2.createServer();
  server.on('stream', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);

    server.on('connection', common.mustCall(() => {
      server.close();
      client.close();
    }));

    const req = client.request();
    req.destroy();
  }));
}

// Test close before connect
{
  const server = h2.createServer();

  server.on('stream', common.mustNotCall());
  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    client.on('close', common.mustCall());
    const socket = client[kSocket];
    socket.on('close', common.mustCall(() => {
      assert(socket.destroyed);
    }));

    const req = client.request();
    // Should throw goaway error
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_GOAWAY_SESSION',
      name: 'Error',
      message: 'New streams cannot be created after receiving a GOAWAY'
    }));

    client.close();
    req.resume();
    req.on('end', common.mustNotCall());
    req.on('close', common.mustCall(() => server.close()));
  }));
}

// Destroy with AbortSignal
{
  const server = h2.createServer();
  const controller = new AbortController();

  server.on('stream', common.mustNotCall());
  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    client.on('close', common.mustCall());

    const { signal } = controller;
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);

    client.on('error', common.mustCall(() => {
      // After underlying stream dies, signal listener detached
      assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    }));

    const req = client.request({}, { signal });

    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ABORT_ERR');
      assert.strictEqual(err.name, 'AbortError');
    }));
    req.on('close', common.mustCall(() => server.close()));

    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, false);
    // Signal listener attached
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);

    controller.abort();

    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, true);
  }));
}
// Pass an already destroyed signal to abort immediately.
{
  const server = h2.createServer();
  const controller = new AbortController();

  server.on('stream', common.mustNotCall());
  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    client.on('close', common.mustCall());

    const { signal } = controller;
    controller.abort();

    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);

    client.on('error', common.mustCall(() => {
      // After underlying stream dies, signal listener detached
      assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    }));

    const req = client.request({}, { signal });
    // Signal already aborted, so no event listener attached.
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);

    assert.strictEqual(req.aborted, false);
    // Destroyed on same tick as request made
    assert.strictEqual(req.destroyed, true);

    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ABORT_ERR');
      assert.strictEqual(err.name, 'AbortError');
    }));
    req.on('close', common.mustCall(() => server.close()));
  }));
}


// Destroy ClientHttpSession with AbortSignal
{
  function testH2ConnectAbort(secure) {
    const server = secure ? h2.createSecureServer() : h2.createServer();
    const controller = new AbortController();

    server.on('stream', common.mustNotCall());
    server.listen(0, common.mustCall(() => {
      const { signal } = controller;
      const protocol = secure ? 'https' : 'http';
      const client = h2.connect(`${protocol}://localhost:${server.address().port}`, {
        signal,
      });
      client.on('close', common.mustCall());
      assert.strictEqual(getEventListeners(signal, 'abort').length, 1);

      client.on('error', common.mustCall(common.mustCall((err) => {
        assert.strictEqual(err.code, 'ABORT_ERR');
        assert.strictEqual(err.name, 'AbortError');
      })));

      const req = client.request({}, {});
      assert.strictEqual(getEventListeners(signal, 'abort').length, 1);

      req.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_CANCEL');
        assert.strictEqual(err.name, 'Error');
        assert.strictEqual(req.aborted, false);
        assert.strictEqual(req.destroyed, true);
      }));
      req.on('close', common.mustCall(() => server.close()));

      assert.strictEqual(req.aborted, false);
      assert.strictEqual(req.destroyed, false);
      // Signal listener attached
      assert.strictEqual(getEventListeners(signal, 'abort').length, 1);

      controller.abort();
    }));
  }
  testH2ConnectAbort(false);
  testH2ConnectAbort(true);
}

// Destroy ClientHttp2Stream with AbortSignal
{
  const server = h2.createServer();
  const controller = new AbortController();

  server.on('stream', common.mustCall((stream) => {
    stream.on('error', common.mustNotCall());
    stream.on('close', common.mustCall(() => {
      assert.strictEqual(stream.rstCode, h2.constants.NGHTTP2_CANCEL);
      server.close();
    }));
    controller.abort();
  }));
  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    client.on('close', common.mustCall());

    const { signal } = controller;
    const req = client.request({}, { signal });
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ABORT_ERR');
      assert.strictEqual(err.name, 'AbortError');
      client.close();
    }));
    req.on('close', common.mustCall());
  }));
}
