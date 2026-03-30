// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

// child_process
{
  const spawn = require('child_process').spawn;
  const cmd = common.isWindows ? 'rundll32' : 'ls';
  const cp = spawn(cmd);
  assert.strictEqual(cp._handle.hasRef(), true);
  cp.unref();
  assert.strictEqual(cp._handle.hasRef(), false);
  cp.ref();
  assert.strictEqual(cp._handle.hasRef(), true);
  cp._handle.close(common.mustCall(() =>
    assert.strictEqual(cp._handle.hasRef(), false)));
}


const dgram = require('dgram');
const { kStateSymbol } = require('internal/dgram');

// dgram ipv4
{
  const sock4 = dgram.createSocket('udp4');
  const handle = sock4[kStateSymbol].handle;

  assert.strictEqual(handle.hasRef(), true);
  sock4.unref();
  assert.strictEqual(handle.hasRef(), false);
  sock4.ref();
  assert.strictEqual(handle.hasRef(), true);
  handle.close(common.mustCall(() =>
    assert.strictEqual(handle.hasRef(), false)));
}


// dgram ipv6
{
  const sock6 = dgram.createSocket('udp6');
  const handle = sock6[kStateSymbol].handle;

  assert.strictEqual(handle.hasRef(), true);
  sock6.unref();
  assert.strictEqual(handle.hasRef(), false);
  sock6.ref();
  assert.strictEqual(handle.hasRef(), true);
  handle.close(common.mustCall(() =>
    assert.strictEqual(handle.hasRef(), false)));
}


// pipe
{
  const { Pipe, constants: PipeConstants } = internalBinding('pipe_wrap');
  const handle = new Pipe(PipeConstants.SOCKET);
  assert.strictEqual(handle.hasRef(), true);
  handle.unref();
  assert.strictEqual(handle.hasRef(), false);
  handle.ref();
  assert.strictEqual(handle.hasRef(), true);
  handle.close(common.mustCall(() =>
    assert.strictEqual(handle.hasRef(), false)));
}


// tcp
{
  const net = require('net');
  const server = net.createServer(() => {}).listen(0);
  assert.strictEqual(server._handle.hasRef(), true);
  assert.strictEqual(server._unref, false);
  server.unref();
  assert.strictEqual(server._handle.hasRef(), false);
  assert.strictEqual(server._unref, true);
  server.ref();
  assert.strictEqual(server._handle.hasRef(), true);
  assert.strictEqual(server._unref, false);
  server._handle.close(common.mustCall(() =>
    assert.strictEqual(server._handle.hasRef(), false)));
}

// timers
{
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Timeout').length, 0);
  const timeout = setTimeout(() => {}, 500);
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Timeout').length, 1);
  timeout.unref();
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Timeout').length, 0);
  timeout.ref();
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Timeout').length, 1);

  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Immediate').length, 0);
  const immediate = setImmediate(() => {});
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Immediate').length, 1);
  immediate.unref();
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Immediate').length, 0);
  immediate.ref();
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Immediate').length, 1);
}

// See also test/pseudo-tty/test-handle-wrap-hasref-tty.js
