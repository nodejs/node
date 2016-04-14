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
  const assert = makeAssert('unrefed() not working on process_wrap');
  const spawn = require('child_process').spawn;
  const cmd = common.isWindows ? 'rundll32' : 'ls';
  const cp = spawn(cmd);
  assert(Object.getPrototypeOf(cp._handle).hasOwnProperty('unrefed'), true);
  assert(cp._handle.unrefed(), false);
  cp.unref();
  assert(cp._handle.unrefed(), true);
  cp.ref();
  assert(cp._handle.unrefed(), false);
  cp._handle.close();
  assert(cp._handle.unrefed(), false);
}


// dgram
{
  const assert = makeAssert('unrefed() not working on udp_wrap');
  const dgram = require('dgram');

  const sock4 = dgram.createSocket('udp4');
  assert(Object.getPrototypeOf(sock4._handle).hasOwnProperty('unrefed'), true);
  assert(sock4._handle.unrefed(), false);
  sock4.unref();
  assert(sock4._handle.unrefed(), true);
  sock4.ref();
  assert(sock4._handle.unrefed(), false);
  sock4._handle.close();
  assert(sock4._handle.unrefed(), false);

  const sock6 = dgram.createSocket('udp6');
  assert(Object.getPrototypeOf(sock6._handle).hasOwnProperty('unrefed'), true);
  assert(sock6._handle.unrefed(), false);
  sock6.unref();
  assert(sock6._handle.unrefed(), true);
  sock6.ref();
  assert(sock6._handle.unrefed(), false);
  sock6._handle.close();
  assert(sock6._handle.unrefed(), false);
}


// pipe
{
  const assert = makeAssert('unrefed() not working on pipe_wrap');
  const Pipe = process.binding('pipe_wrap').Pipe;
  const handle = new Pipe();
  assert(Object.getPrototypeOf(handle).hasOwnProperty('unrefed'), true);
  assert(handle.unrefed(), false);
  handle.unref();
  assert(handle.unrefed(), true);
  handle.ref();
  assert(handle.unrefed(), false);
  handle.close();
  assert(handle.unrefed(), false);
}


// tcp
{
  const assert = makeAssert('unrefed() not working on tcp_wrap');
  const net = require('net');
  const server = net.createServer(() => {}).listen(common.PORT);
  assert(Object.getPrototypeOf(server._handle).hasOwnProperty('unrefed'), true);
  assert(server._handle.unrefed(), false);
  assert(server._unref, false);
  server.unref();
  assert(server._handle.unrefed(), true);
  assert(server._unref, true);
  server.ref();
  assert(server._handle.unrefed(), false);
  assert(server._unref, false);
  server._handle.close();
  assert(server._handle.unrefed(), false);
}


// timers
{
  const assert = makeAssert('unrefed() not working on timer_wrap');
  const timer = setTimeout(() => {}, 500);
  timer.unref();
  assert(Object.getPrototypeOf(timer._handle).hasOwnProperty('unrefed'), true);
  assert(timer._handle.unrefed(), true);
  timer.ref();
  assert(timer._handle.unrefed(), false);
  timer.close();
  assert(timer._handle.unrefed(), false);
}
