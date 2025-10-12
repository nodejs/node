'use strict';
const common = require('../common');
const Countdown = require('../common/countdown');
const assert = require('assert');
const net = require('net');
const dc = require('diagnostics_channel');

const isNetSocket = (socket) => socket instanceof net.Socket;
const isNetServer = (server) => server instanceof net.Server;

function testDiagnosticChannel(subscribers, test, after) {
  dc.tracingChannel('net.server.listen').subscribe(subscribers);

  test(common.mustCall(() => {
    dc.tracingChannel('net.server.listen').unsubscribe(subscribers);
    after?.();
  }));
}

const testSuccessfulListen = common.mustCall(() => {
  let cb;
  const netClientSocketCount = 3;
  const countdown = new Countdown(netClientSocketCount, () => {
    server.close();
    cb();
  });
  const server = net.createServer(common.mustCall((socket) => {
    socket.destroy();
    countdown.dec();
  }, netClientSocketCount));

  dc.subscribe('net.client.socket', common.mustCall(({ socket }) => {
    assert.strictEqual(isNetSocket(socket), true);
  }, netClientSocketCount));

  dc.subscribe('net.server.socket', common.mustCall(({ socket }) => {
    assert.strictEqual(isNetSocket(socket), true);
  }, netClientSocketCount));

  testDiagnosticChannel(
    {
      asyncStart: common.mustCall(({ server: currentServer, options }) => {
        assert.strictEqual(isNetServer(server), true);
        assert.strictEqual(currentServer, server);
        assert.strictEqual(options.customOption, true);
      }),
      asyncEnd: common.mustCall(({ server: currentServer }) => {
        assert.strictEqual(isNetServer(server), true);
        assert.strictEqual(currentServer, server);
      }),
      error: common.mustNotCall()
    },
    common.mustCall((callback) => {
      cb = callback;
      server.listen({ port: 0, customOption: true }, () => {
        // All supported ways of creating a net client socket connection.
        const { port } = server.address();
        net.connect(port);

        net.createConnection(port);

        new net.Socket().connect(port);
      });
    }),
    testFailingListen
  );
});

const testFailingListen = common.mustCall(() => {
  const originalServer = net.createServer(common.mustNotCall());

  originalServer.listen(() => {
    const server = net.createServer(common.mustNotCall());

    testDiagnosticChannel(
      {
        asyncStart: common.mustCall(({ server: currentServer, options }) => {
          assert.strictEqual(isNetServer(server), true);
          assert.strictEqual(currentServer, server);
          assert.strictEqual(options.customOption, true);
        }),
        asyncEnd: common.mustNotCall(),
        error: common.mustCall(({ server: currentServer }) => {
          assert.strictEqual(isNetServer(server), true);
          assert.strictEqual(currentServer, server);
        }),
      },
      common.mustCall((callback) => {
        server.on('error', () => {});
        server.listen({ port: originalServer.address().port, customOption: true });
        callback();
      }),
      common.mustCall(() => {
        originalServer.close();
        server.close();
      })
    );
  });
});

testSuccessfulListen();
