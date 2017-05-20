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
validateNormalizedArgs([], [{}, null]);
validateNormalizedArgs([{ port: 1234 }], [{ port: 1234 }, null]);
validateNormalizedArgs([{ port: 1234 }, assert.fail],
                       [{ port: 1234 }, assert.fail]);

// Connecting to the server should fail with a standard array.
{
  const server = net.createServer(common.mustNotCall('should not connect'));

  server.listen(common.mustCall(() => {
    const possibleErrors = ['ECONNREFUSED', 'EADDRNOTAVAIL'];
    const port = server.address().port;
    const socket = new net.Socket();

    socket.on('error', common.mustCall((err) => {
      assert(possibleErrors.includes(err.code));
      assert(possibleErrors.includes(err.errno));
      assert.strictEqual(err.syscall, 'connect');
      server.close();
    }));

    socket.connect([{ port }, assert.fail]);
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
