'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const truthyValues = [true, 1, 'true', {}, []];
const delays = [[123, 0], [456123, 456], [-123000, 0], [undefined, 0]];
const falseyValues = [false, 0, ''];

const genSetKeepAlive = (desiredEnable, desiredDelay) => (enable, delay) => {
  assert.strictEqual(enable, desiredEnable);
  assert.strictEqual(delay, desiredDelay);
};

for (const value of truthyValues) {
  for (const delay of delays) {
    const server = net.createServer();

    server.listen(0, common.mustCall(function() {
      const port = server.address().port;

      const client = net.connect(
        { port, keepAlive: value, keepAliveInitialDelay: delay[0] },
        common.mustCall(() => client.end())
      );

      client._handle.setKeepAlive = common.mustCall(
        genSetKeepAlive(true, delay[1])
      );

      client.on('end', common.mustCall(function() {
        server.close();
      }));
    }));
  }
}

for (const value of falseyValues) {
  const server = net.createServer();

  server.listen(0, common.mustCall(function() {
    const port = server.address().port;

    const client = net.connect(
      { port, keepAlive: value },
      common.mustCall(() => client.end())
    );

    client._handle.setKeepAlive = common.mustNotCall();

    client.on('end', common.mustCall(function() {
      server.close();
    }));
  }));
}
