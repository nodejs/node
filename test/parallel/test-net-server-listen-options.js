'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const util = require('util');
const fs = require('fs');
const uv = process.binding('uv');
const TCP = process.binding('tcp_wrap').TCP;
const Pipe = process.binding('pipe_wrap').Pipe;

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
  net.createServer().listen({port: 0})
    .on('listening', common.mustCall(close));
}

let counter = 0;

function randomPipePath() {
  return common.PIPE + '-listen-pipe-' + (counter++);
}

function randomHandle(type) {
  let handle, err;
  if (type === 'tcp') {
    handle = new TCP();
    err = handle.bind('0.0.0.0', 0);
    if (err < 0) {  // uv.errname requires err < 0
      assert(err >= 0, `unable to bind arbitrary tcp port: ${uv.errname(err)}`);
    }
  } else {
    const path = randomPipePath();
    handle = new Pipe();
    err = handle.bind(path);
    if (err < 0) {  // uv.errname requires err < 0
      assert(err >= 0, `unable to bind pipe ${path}: ${uv.errname(err)}`);
    }
  }
  return handle;
}

{
  // Test listen(path)
  net.createServer()
    .listen(randomPipePath())
    .on('listening', common.mustCall(close));
  // Test listen({path})
  net.createServer()
    .listen({path: randomPipePath()}, common.mustCall(close));
  // Test listen(path, cb)
  net.createServer()
    .listen(randomPipePath(), common.mustCall(close));
  // Test listen(path, cb)
  net.createServer()
    .listen({path: randomPipePath()}, common.mustCall(close));
}

// Not a public API, used by child_process
{
  // Test listen(handle)
  net.createServer()
    .listen(randomHandle('tcp'))
    .on('listening', common.mustCall(close));
  net.createServer()
    .listen(randomHandle('pipe'))
    .on('listening', common.mustCall(close));
}

// Not a public API, used by child_process
{
  // Test listen(handle, cb)
  net.createServer()
    .listen(randomHandle('tcp'), common.mustCall(close));
  net.createServer()
    .listen(randomHandle('pipe'), common.mustCall(close));
}

{
  // Test listen({handle}, cb)
  net.createServer()
    .listen({handle: randomHandle('tcp')}, common.mustCall(close));
  net.createServer()
    .listen({handle: randomHandle('pipe')}, common.mustCall(close));
}

{
  // Test listen({_handle: handle}, cb)
  net.createServer()
    .listen({_handle: randomHandle('tcp')}, common.mustCall(close));
  net.createServer()
    .listen({_handle: randomHandle('pipe')}, common.mustCall(close));
}

{
  // Test listen({fd: handle})
  net.createServer()
    .listen({fd: randomHandle('tcp').fd})
    .on('listening', common.mustCall(close));
  net.createServer()
    .listen({fd: randomHandle('pipe').fd})
    .on('listening', common.mustCall(close));
}

{
  // Test listen({fd: handle}, cb)
  net.createServer()
    .listen({fd: randomHandle('tcp').fd}, common.mustCall(close));
  net.createServer()
    .listen({fd: randomHandle('pipe').fd}, common.mustCall(close));
}

{
  // Test invalid fd
  const fd = fs.openSync(__filename, 'r');
  net.createServer()
    .listen({fd: fd}, common.mustNotCall())
    .on('error', common.mustCall(function(err) {
      assert.strictEqual(err + '', 'Error: listen EINVAL');
      this.close();
    }));
}

// Test listen(port, cb) and listen({port: port}, cb) combinations
const listenOnPort = [
  (port, cb) => net.createServer().listen({port}, cb),
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
