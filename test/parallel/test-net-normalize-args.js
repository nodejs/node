'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const net = require('net');
const { normalizedArgsSymbol } = require('internal/net');

function validateNormalizedArgs(input, output) {
  const args = net._normalizeArgs(input);

  assert.deepStrictEqual(args, output);
  assert.strictEqual(args[normalizedArgsSymbol], true);
}

// Test creation of normalized arguments.
const res = [{}, null];
res[normalizedArgsSymbol] = true;
validateNormalizedArgs([], res);
res[0].port = 1234;
validateNormalizedArgs([{ port: 1234 }], res);
res[1] = assert.fail;
validateNormalizedArgs([{ port: 1234 }, assert.fail], res);

// Connecting to the server should fail with a standard array.
{
  const server = net.createServer(common.mustNotCall('should not connect'));

  server.listen(common.mustCall(() => {
    const port = server.address().port;
    const socket = new net.Socket();

    assert.throws(() => {
      socket.connect([{ port }, assert.fail]);
    }, {
      code: 'ERR_MISSING_ARGS'
    });
    server.close();
  }));
}

// Connecting to the server should succeed with a normalized array.
{
  const server = net.createServer(common.mustCall((connection) => {
    connection.end();
    server.close();
  }));

  server.listen(common.mustCall(() => {
    const port = server.address().port;
    const socket = new net.Socket();
    const args = net._normalizeArgs([{ port }, common.mustCall()]);

    socket.connect(args);
  }));
}
