'use strict';

require('../common');
const assert = require('assert');
const net = require('net');

// The keep-alive delays are given in milliseconds but the underlying socket
// options are configured in whole seconds, and uv_tcp_keepalive() rejects a
// delay outside [1, 32767]. Verifies that a delay which cannot be applied as
// requested is rejected instead of being silently altered.

const MAX_DELAY = 32767 * 1000;

// A positive delay below 1000 ms would be truncated to 0 seconds, which leaves
// the system default in place instead of applying the requested timing.
for (const initialDelay of [1, 400, 999]) {
  assert.throws(() => net.Socket.prototype.setKeepAlive.call(
    {}, true, initialDelay), {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: /The value of "initialDelay" is out of range/,
  });
}

// The interval is validated the same way.
assert.throws(() => net.Socket.prototype.setKeepAlive.call(
  {}, true, 5000, 500), {
  code: 'ERR_OUT_OF_RANGE',
  message: /The value of "interval" is out of range/,
});

// A delay above the maximum cannot be carried by the socket options.
assert.throws(() => net.Socket.prototype.setKeepAlive.call(
  {}, true, MAX_DELAY + 1000), {
  code: 'ERR_OUT_OF_RANGE',
  message: /The value of "initialDelay" is out of range/,
});

// The options object form is validated as well.
assert.throws(() => net.Socket.prototype.setKeepAlive.call(
  {}, { enable: true, initialDelay: 999 }), {
  code: 'ERR_OUT_OF_RANGE',
  message: /The value of "initialDelay" is out of range/,
});

// A non-numeric delay is rejected by the same check.
assert.throws(() => net.Socket.prototype.setKeepAlive.call(
  {}, true, '1000'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Delays that can be applied as requested are accepted. A non-positive value
// keeps its documented meaning of leaving the current setting unchanged, and
// so does omitting it entirely.
const server = net.createServer();
server.listen(0, () => {
  const client = net.connect({ port: server.address().port }, () => {
    for (const args of [
      [true, 1000],
      [true, MAX_DELAY],
      [true, 5000, 1000],
      [true, 0],
      [true, -1],
      [true],
      // Agent passes Infinity to mean "no timeout".
      [true, Infinity],
      [{ enable: true, initialDelay: 1000 }],
      // Nothing is configured while keep-alive is disabled, so a delay that
      // could not be applied is not rejected either.
      [false, 400],
    ]) {
      client.setKeepAlive(...args);
    }

    client.end();
  });

  client.on('end', () => server.close());
});
