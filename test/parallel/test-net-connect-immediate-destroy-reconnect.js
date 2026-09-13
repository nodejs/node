'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// Regression test for https://github.com/nodejs/node/issues/55519.
//
// Destroying a socket while its DNS lookup is still in flight and then
// reconnecting on 'close' must not let the stale lookup callback from the
// destroyed attempt drive internalConnect on the new attempt's handle. Doing
// so would issue a second connect on the same handle and fail the connection
// with EALREADY (EINVAL on Windows).
//
// The flow is driven deterministically: the custom lookup captures the
// callbacks of both attempts and the stale (attempt 1) callback is invoked
// before the callback of the current (attempt 2) attempt.

const cases = [
  { autoSelectFamily: false },
  { autoSelectFamily: true },
];

function runCase(options, done) {
  let connected = false;
  let accepted = false;

  const watchdog = setTimeout(() => {
    console.error(`test-case timed out: ${JSON.stringify(options)}`);
    process.exit(1);
  }, 10_000);

  const socket = new net.Socket();

  const finish = common.mustCall(() => {
    clearTimeout(watchdog);
    socket.destroy();
    server.close();
    done();
  });

  // Teardown only after both the client has connected and the server has
  // accepted the connection, so the accept callback is never dropped by an
  // early server.close().
  const server = net.createServer(common.mustCall(() => {
    accepted = true;
    if (connected) {
      finish();
    }
  }));

  server.listen(0, common.localhostIPv4, common.mustCall(() => {
    const port = server.address().port;
    const lookupCalls = [];
    let reconnected = false;
    let connectionAttempts = 0;

    function lookup(host, dnsopts, cb) {
      lookupCalls.push({ dnsopts, cb });
    }

    socket.on('connectionAttempt', () => {
      connectionAttempts++;
    });

    socket.on('connect', common.mustCall(() => {
      // Only the current (second) attempt may have connected: the stale
      // callback must not have started a connect on the new attempt's handle.
      assert.strictEqual(connectionAttempts, 1);
      connected = true;
      if (accepted) {
        finish();
      }
    }));

    socket.on('error', common.mustNotCall());

    socket.on('close', common.mustCallAtLeast(() => {
      if (reconnected) {
        return;
      }
      reconnected = true;
      // Start a new connection attempt; the lookup callback of the first
      // attempt is still pending.
      socket.connect({ host: 'host.example', port, lookup, ...options });
      assert.strictEqual(lookupCalls.length, 2);

      const fire = (call) => {
        if (call.dnsopts.all === true) {
          call.cb(null, [{ address: common.localhostIPv4, family: 4 }]);
        } else {
          call.cb(null, common.localhostIPv4, 4);
        }
      };
      // Invoke the stale callback first, then the current attempt's callback.
      // The stale callback must be ignored.
      fire(lookupCalls[0]);
      fire(lookupCalls[1]);
    }));

    socket.connect({ host: 'host.example', port, lookup, ...options });
    socket.destroy();
  }));
}

let index = 0;
function next() {
  if (index >= cases.length) {
    return;
  }
  runCase(cases[index++], next);
}

next();
