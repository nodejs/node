'use strict';

const common = require('../common');
const strictEqual = require('assert').strictEqual;

function makeAssert(message) {
  return function(actual, expected) {
    strictEqual(actual, expected, message);
  };
}


// child_process
{
  const assert = makeAssert('hasRef() not working on process_wrap');
  const spawn = require('child_process').spawn;
  const cmd = common.isWindows ? 'rundll32' : 'ls';
  const cp = spawn(cmd);
  assert(Object.getPrototypeOf(cp._handle).hasOwnProperty('hasRef'), true);
  assert(cp._handle.hasRef(), true);
  cp.unref();
  assert(cp._handle.hasRef(), false);
  cp.ref();
  assert(cp._handle.hasRef(), true);
  cp._handle.close(common.mustCall(() => assert(cp._handle.hasRef(), false)));
}


// dgram
{
  const assert = makeAssert('hasRef() not working on udp_wrap');
  const dgram = require('dgram');

  const sock4 = dgram.createSocket('udp4');
  assert(Object.getPrototypeOf(sock4._handle).hasOwnProperty('hasRef'), true);
  assert(sock4._handle.hasRef(), true);
  sock4.unref();
  assert(sock4._handle.hasRef(), false);
  sock4.ref();
  assert(sock4._handle.hasRef(), true);
  sock4._handle.close(
      common.mustCall(() => assert(sock4._handle.hasRef(), false)));

  const sock6 = dgram.createSocket('udp6');
  assert(Object.getPrototypeOf(sock6._handle).hasOwnProperty('hasRef'), true);
  assert(sock6._handle.hasRef(), true);
  sock6.unref();
  assert(sock6._handle.hasRef(), false);
  sock6.ref();
  assert(sock6._handle.hasRef(), true);
  sock6._handle.close(
      common.mustCall(() => assert(sock6._handle.hasRef(), false)));
}


// pipe
{
  const assert = makeAssert('hasRef() not working on pipe_wrap');
  const Pipe = process.binding('pipe_wrap').Pipe;
  const handle = new Pipe();
  assert(Object.getPrototypeOf(handle).hasOwnProperty('hasRef'), true);
  assert(handle.hasRef(), true);
  handle.unref();
  assert(handle.hasRef(), false);
  handle.ref();
  assert(handle.hasRef(), true);
  handle.close(common.mustCall(() => assert(handle.hasRef(), false)));
}


// tcp
{
  const assert = makeAssert('hasRef() not working on tcp_wrap');
  const net = require('net');
  const server = net.createServer(() => {}).listen(0);
  assert(Object.getPrototypeOf(server._handle).hasOwnProperty('hasRef'), true);
  assert(server._handle.hasRef(), true);
  assert(server._unref, false);
  server.unref();
  assert(server._handle.hasRef(), false);
  assert(server._unref, true);
  server.ref();
  assert(server._handle.hasRef(), true);
  assert(server._unref, false);
  server._handle.close(
      common.mustCall(() => assert(server._handle.hasRef(), false)));
}


// timers
{
  const assert = makeAssert('hasRef() not working on timer_wrap');
  const timer = setTimeout(() => {}, 500);
  timer.unref();
  assert(Object.getPrototypeOf(timer._handle).hasOwnProperty('hasRef'), true);
  assert(timer._handle.hasRef(), false);
  timer.ref();
  assert(timer._handle.hasRef(), true);
  timer._handle.close(
      common.mustCall(() => assert(timer._handle.hasRef(), false)));
}
