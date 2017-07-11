'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const util = require('util');

function close() { this.close(); }

function listenError(literals, ...values) {
  let result = literals[0];
  for (const [i, value] of values.entries()) {
    const str = util.inspect(value);
    // Need to escape special characters.
    result += str.replace(/[\\^$.*+?()[\]{}|=!<>:-]/g, '\\$&');
    result += literals[i + 1];
  }
  return new RegExp(`Error: Invalid listen argument: ${result}`);
}

{
  // Test listen()
  net.createServer().listen().on('listening', common.mustCall(close));
  // Test listen(cb)
  net.createServer().listen(common.mustCall(close));
  // Test listen(port)
  net.createServer().listen(0).on('listening', common.mustCall(close));
  // Test listen({port})
  net.createServer().listen({ port: 0 })
    .on('listening', common.mustCall(close));
}

// Test listen(port, cb) and listen({port: port}, cb) combinations
const listenOnPort = [
  (port, cb) => net.createServer().listen({ port }, cb),
  (port, cb) => net.createServer().listen(port, cb)
];

{
  const portError = /^RangeError: "port" argument must be >= 0 and < 65536$/;
  for (const listen of listenOnPort) {
    // Arbitrary unused ports
    listen('0', common.mustCall(close));
    listen(0, common.mustCall(close));
    listen(undefined, common.mustCall(close));
    // Test invalid ports
    assert.throws(() => listen(-1, common.mustNotCall()), portError);
    assert.throws(() => listen(NaN, common.mustNotCall()), portError);
    assert.throws(() => listen(123.456, common.mustNotCall()), portError);
    assert.throws(() => listen(65536, common.mustNotCall()), portError);
    assert.throws(() => listen(1 / 0, common.mustNotCall()), portError);
    assert.throws(() => listen(-1 / 0, common.mustNotCall()), portError);
  }
  // In listen(options, cb), port takes precedence over path
  assert.throws(() => {
    net.createServer().listen({ port: -1, path: common.PIPE },
                              common.mustNotCall());
  }, portError);
}

{
  function shouldFailToListen(options, optionsInMessage) {
    // Plain arguments get normalized into an object before
    // formatted in message, options objects don't.
    if (arguments.length === 1) {
      optionsInMessage = options;
    }
    const block = () => {
      net.createServer().listen(options, common.mustNotCall());
    };
    assert.throws(block, listenError`${optionsInMessage}`,
                  `expect listen(${util.inspect(options)}) to throw`);
  }

  shouldFailToListen(null, { port: null });
  shouldFailToListen({ port: null });
  shouldFailToListen(false, { port: false });
  shouldFailToListen({ port: false });
  shouldFailToListen(true, { port: true });
  shouldFailToListen({ port: true });
  // Invalid fd as listen(handle)
  shouldFailToListen({ fd: -1 });
  // Invalid path in listen(options)
  shouldFailToListen({ path: -1 });
  // Host without port
  shouldFailToListen({ host: 'localhost' });
}
