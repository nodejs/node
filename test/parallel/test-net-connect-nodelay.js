'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const truthyValues = [true, 1, 'true', {}, []];
const falseyValues = [false, 0, ''];
const genSetNoDelay = (desiredArg) => (enable) => {
  assert.strictEqual(enable, desiredArg);
};

for (const value of truthyValues) {
  const server = net.createServer();

  server.listen(0, common.mustCall(function() {
    const port = server.address().port;

    const client = net.connect(
      { port, noDelay: value },
      common.mustCall(() => client.end())
    );

    client._handle.setNoDelay = common.mustCall(genSetNoDelay(true));

    client.on('end', common.mustCall(function() {
      server.close();
    }));
  }));
}

for (const value of falseyValues) {
  const server = net.createServer();

  server.listen(0, common.mustCall(function() {
    const port = server.address().port;

    const client = net.connect(
      { port, noDelay: value },
      common.mustCall(() => client.end())
    );

    client._handle.setNoDelay = common.mustNotCall();

    client.on('end', common.mustCall(function() {
      server.close();
    }));
  }));
}
