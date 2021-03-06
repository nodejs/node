// Flags: --expose-internals
'use strict';

const common = require('../common');
const strictEqual = require('assert').strictEqual;
const { internalBinding } = require('internal/test/binding');

// child_process
{
  const spawn = require('child_process').spawn;
  const cmd = common.isWindows ? 'rundll32' : 'ls';
  const cp = spawn(cmd);
  strictEqual(cp._handle.hasRef(),
              true, 'process_wrap: not initially refed');
  cp.unref();
  strictEqual(cp._handle.hasRef(),
              false, 'process_wrap: unref() ineffective');
  cp.ref();
  strictEqual(cp._handle.hasRef(),
              true, 'process_wrap: ref() ineffective');
  cp._handle.close(common.mustCall(() =>
    strictEqual(cp._handle.hasRef(),
                false, 'process_wrap: not unrefed on close')));
}


const dgram = require('dgram');
const { kStateSymbol } = require('internal/dgram');

// dgram ipv4
{
  const sock4 = dgram.createSocket('udp4');
  const handle = sock4[kStateSymbol].handle;

  strictEqual(handle.hasRef(),
              true, 'udp_wrap: ipv4: not initially refed');
  sock4.unref();
  strictEqual(handle.hasRef(),
              false, 'udp_wrap: ipv4: unref() ineffective');
  sock4.ref();
  strictEqual(handle.hasRef(),
              true, 'udp_wrap: ipv4: ref() ineffective');
  handle.close(common.mustCall(() =>
    strictEqual(handle.hasRef(),
                false, 'udp_wrap: ipv4: not unrefed on close')));
}


// dgram ipv6
{
  const sock6 = dgram.createSocket('udp6');
  const handle = sock6[kStateSymbol].handle;

  strictEqual(handle.hasRef(),
              true, 'udp_wrap: ipv6: not initially refed');
  sock6.unref();
  strictEqual(handle.hasRef(),
              false, 'udp_wrap: ipv6: unref() ineffective');
  sock6.ref();
  strictEqual(handle.hasRef(),
              true, 'udp_wrap: ipv6: ref() ineffective');
  handle.close(common.mustCall(() =>
    strictEqual(handle.hasRef(),
                false, 'udp_wrap: ipv6: not unrefed on close')));
}


// pipe
{
  const { Pipe, constants: PipeConstants } = internalBinding('pipe_wrap');
  const handle = new Pipe(PipeConstants.SOCKET);
  strictEqual(handle.hasRef(),
              true, 'pipe_wrap: not initially refed');
  handle.unref();
  strictEqual(handle.hasRef(),
              false, 'pipe_wrap: unref() ineffective');
  handle.ref();
  strictEqual(handle.hasRef(),
              true, 'pipe_wrap: ref() ineffective');
  handle.close(common.mustCall(() =>
    strictEqual(handle.hasRef(),
                false, 'pipe_wrap: not unrefed on close')));
}


// tcp
{
  const net = require('net');
  const server = net.createServer(() => {}).listen(0);
  strictEqual(server._handle.hasRef(),
              true, 'tcp_wrap: not initially refed');
  strictEqual(server._unref,
              false, 'tcp_wrap: _unref initially incorrect');
  server.unref();
  strictEqual(server._handle.hasRef(),
              false, 'tcp_wrap: unref() ineffective');
  strictEqual(server._unref,
              true, 'tcp_wrap: _unref not updated on unref()');
  server.ref();
  strictEqual(server._handle.hasRef(),
              true, 'tcp_wrap: ref() ineffective');
  strictEqual(server._unref,
              false, 'tcp_wrap: _unref not updated on ref()');
  server._handle.close(common.mustCall(() =>
    strictEqual(server._handle.hasRef(),
                false, 'tcp_wrap: not unrefed on close')));
}


// See also test/pseudo-tty/test-handle-wrap-isrefed-tty.js
