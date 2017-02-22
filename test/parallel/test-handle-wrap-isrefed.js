'use strict';

const common = require('../common');
const strictEqual = require('assert').strictEqual;

// child_process
{
  const spawn = require('child_process').spawn;
  const cmd = common.isWindows ? 'rundll32' : 'ls';
  const cp = spawn(cmd);
  strictEqual(Object.getPrototypeOf(cp._handle).hasOwnProperty('hasRef'),
              true, 'process_wrap: hasRef() missing');
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


// dgram ipv4
{
  const dgram = require('dgram');
  const sock4 = dgram.createSocket('udp4');
  strictEqual(Object.getPrototypeOf(sock4._handle).hasOwnProperty('hasRef'),
              true, 'udp_wrap: ipv4: hasRef() missing');
  strictEqual(sock4._handle.hasRef(),
              true, 'udp_wrap: ipv4: not initially refed');
  sock4.unref();
  strictEqual(sock4._handle.hasRef(),
              false, 'udp_wrap: ipv4: unref() ineffective');
  sock4.ref();
  strictEqual(sock4._handle.hasRef(),
              true, 'udp_wrap: ipv4: ref() ineffective');
  sock4._handle.close(common.mustCall(() =>
      strictEqual(sock4._handle.hasRef(),
                  false, 'udp_wrap: ipv4: not unrefed on close')));
}


// dgram ipv6
{
  const dgram = require('dgram');
  const sock6 = dgram.createSocket('udp6');
  strictEqual(Object.getPrototypeOf(sock6._handle).hasOwnProperty('hasRef'),
              true, 'udp_wrap: ipv6: hasRef() missing');
  strictEqual(sock6._handle.hasRef(),
              true, 'udp_wrap: ipv6: not initially refed');
  sock6.unref();
  strictEqual(sock6._handle.hasRef(),
              false, 'udp_wrap: ipv6: unref() ineffective');
  sock6.ref();
  strictEqual(sock6._handle.hasRef(),
              true, 'udp_wrap: ipv6: ref() ineffective');
  sock6._handle.close(common.mustCall(() =>
      strictEqual(sock6._handle.hasRef(),
                  false, 'udp_wrap: ipv6: not unrefed on close')));
}


// pipe
{
  const Pipe = process.binding('pipe_wrap').Pipe;
  const handle = new Pipe();
  strictEqual(Object.getPrototypeOf(handle).hasOwnProperty('hasRef'),
              true, 'pipe_wrap: hasRef() missing');
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
  strictEqual(Object.getPrototypeOf(server._handle).hasOwnProperty('hasRef'),
              true, 'tcp_wrap: hasRef() missing');
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


// timers
{
  const timer = setTimeout(() => {}, 500);
  timer.unref();
  strictEqual(Object.getPrototypeOf(timer._handle).hasOwnProperty('hasRef'),
              true, 'timer_wrap: hasRef() missing');
  strictEqual(timer._handle.hasRef(),
              false, 'timer_wrap: unref() ineffective');
  timer.ref();
  strictEqual(timer._handle.hasRef(),
              true, 'timer_wrap: ref() ineffective');
  timer._handle.close(common.mustCall(() =>
      strictEqual(timer._handle.hasRef(),
                  false, 'timer_wrap: not unrefed on close')));
}


// see also test/pseudo-tty/test-handle-wrap-isrefed-tty.js
