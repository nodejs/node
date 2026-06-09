'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// Verifies that setKeepAlive() forwards the keepalive probe interval
// (TCP_KEEPINTVL) and probe count (TCP_KEEPCNT) to the handle. The interval is
// converted from milliseconds to whole seconds, mirroring the initialDelay
// handling and uv_tcp_keepalive_ex().

// Explicit setKeepAlive(enable, initialDelay, interval, count) forwards all
// four values, converting milliseconds to seconds for the delays.
{
  const server = net.createServer();
  server.listen(0, common.mustCall(() => {
    const client = net.connect(
      { port: server.address().port },
      common.mustCall(() => {
        client._handle.setKeepAlive = common.mustCall(
          (enable, delay, interval, count) => {
            assert.strictEqual(enable, true);
            assert.strictEqual(delay, 5);
            assert.strictEqual(interval, 10);
            assert.strictEqual(count, 9);
          });
        client.setKeepAlive(true, 5000, 10000, 9);
        client.end();
      }));

    client.on('end', common.mustCall(() => server.close()));
  }));
}

// Omitting interval/count passes undefined through; the handle applies the
// libuv defaults (1s interval, 10 probes).
{
  const server = net.createServer();
  server.listen(0, common.mustCall(() => {
    const client = net.connect(
      { port: server.address().port },
      common.mustCall(() => {
        client._handle.setKeepAlive = common.mustCall(
          (enable, delay, interval, count) => {
            assert.strictEqual(enable, true);
            assert.strictEqual(delay, 5);
            assert.strictEqual(interval, undefined);
            assert.strictEqual(count, undefined);
          });
        client.setKeepAlive(true, 5000);
        client.end();
      }));

    client.on('end', common.mustCall(() => server.close()));
  }));
}
