'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// The keep-alive delays are given in milliseconds but the underlying socket
// options are configured in whole seconds. A positive value below 1000 ms is
// truncated to 0 seconds, which leaves the system default in place instead of
// applying the requested timing. Verifies that this emits a warning rather
// than being silently ignored.

// Warnings are emitted on the process, so the cases run one at a time to keep
// each one's warnings from being observed by the others.
const cases = [
  // A delay below 1000 ms is truncated to 0 and warns.
  {
    configure: (client) => client.setKeepAlive(true, 400),
    check: common.mustCall((messages) => {
      assert.strictEqual(messages.length, 1);
      assert.match(messages[0], /initialDelay of 400 ms/);
      assert.match(messages[0], /at least 1000 ms/);
    }),
  },
  // The interval is truncated the same way and warns independently.
  {
    configure: (client) => client.setKeepAlive(true, 5000, 500),
    check: common.mustCall((messages) => {
      assert.strictEqual(messages.length, 1);
      assert.match(messages[0], /interval of 500 ms/);
    }),
  },
  // Both delays can be truncated by the same call.
  {
    configure: (client) => client.setKeepAlive(true, 400, 500),
    check: common.mustCall((messages) => {
      assert.strictEqual(messages.length, 2);
      assert.match(messages[0], /initialDelay of 400 ms/);
      assert.match(messages[1], /interval of 500 ms/);
    }),
  },
  // The options object form warns as well.
  {
    configure: (client) => client.setKeepAlive({
      enable: true,
      initialDelay: 999,
    }),
    check: common.mustCall((messages) => {
      assert.strictEqual(messages.length, 1);
      assert.match(messages[0], /initialDelay of 999 ms/);
    }),
  },
  // A delay of at least 1000 ms is applied as requested and does not warn.
  {
    configure: (client) => client.setKeepAlive(true, 1000),
    check: common.mustCall((messages) => assert.deepStrictEqual(messages, [])),
  },
  // 0 means "leave the current value unchanged" and is not a truncation.
  {
    configure: (client) => client.setKeepAlive(true, 0),
    check: common.mustCall((messages) => assert.deepStrictEqual(messages, [])),
  },
  // Omitting the delay does not warn.
  {
    configure: (client) => client.setKeepAlive(true),
    check: common.mustCall((messages) => assert.deepStrictEqual(messages, [])),
  },
  // Nothing is configured when keep-alive is disabled, so there is nothing to
  // warn about.
  {
    configure: (client) => client.setKeepAlive(false, 400),
    check: common.mustCall((messages) => assert.deepStrictEqual(messages, [])),
  },
];

function runCase({ configure, check }, done) {
  const messages = [];
  const onWarning = (warning) => {
    if (warning.name === 'KeepAliveWarning') messages.push(warning.message);
  };
  process.on('warning', onWarning);

  const server = net.createServer();
  server.listen(0, common.mustCall(() => {
    const client = net.connect(
      { port: server.address().port },
      common.mustCall(() => {
        configure(client);
        client.end();
      }));

    client.on('end', common.mustCall(() => {
      server.close(common.mustCall(() => {
        // Warnings are emitted on the next tick.
        setImmediate(() => {
          process.removeListener('warning', onWarning);
          check(messages);
          done();
        });
      }));
    }));
  }));
}

(function next(i) {
  if (i === cases.length) return;
  runCase(cases[i], common.mustCall(() => next(i + 1)));
})(0);
