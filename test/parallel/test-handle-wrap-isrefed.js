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
  const assert = makeAssert('isRefed() not working on process_wrap');
  const spawn = require('child_process').spawn;
  const cmd = common.isWindows ? 'rundll32' : 'ls';
  const cp = spawn(cmd);
  assert(Object.getPrototypeOf(cp._handle).hasOwnProperty('isRefed'), true);
  assert(cp._handle.isRefed(), true);
  cp.unref();
  assert(cp._handle.isRefed(), false);
  cp.ref();
  assert(cp._handle.isRefed(), true);
  cp.unref();
}


// dgram
{
  const assert = makeAssert('isRefed() not working on udp_wrap');
  const dgram = require('dgram');

  const sock4 = dgram.createSocket('udp4');
  assert(Object.getPrototypeOf(sock4._handle).hasOwnProperty('isRefed'), true);
  assert(sock4._handle.isRefed(), true);
  sock4.unref();
  assert(sock4._handle.isRefed(), false);
  sock4.ref();
  assert(sock4._handle.isRefed(), true);
  sock4.unref();

  const sock6 = dgram.createSocket('udp6');
  assert(Object.getPrototypeOf(sock6._handle).hasOwnProperty('isRefed'), true);
  assert(sock6._handle.isRefed(), true);
  sock6.unref();
  assert(sock6._handle.isRefed(), false);
  sock6.ref();
  assert(sock6._handle.isRefed(), true);
  sock6.unref();
}


// pipe
{
  const assert = makeAssert('isRefed() not working on pipe_wrap');
  const Pipe = process.binding('pipe_wrap').Pipe;
  const handle = new Pipe();
  assert(Object.getPrototypeOf(handle).hasOwnProperty('isRefed'), true);
  assert(handle.isRefed(), true);
  handle.unref();
  assert(handle.isRefed(), false);
  handle.ref();
  assert(handle.isRefed(), true);
  handle.unref();
}


// tcp
{
  const assert = makeAssert('isRefed() not working on tcp_wrap');
  const net = require('net');
  const server = net.createServer(() => {}).listen(common.PORT);
  assert(Object.getPrototypeOf(server._handle).hasOwnProperty('isRefed'), true);
  assert(server._handle.isRefed(), true);
  assert(server._unref, false);
  server.unref();
  assert(server._handle.isRefed(), false);
  assert(server._unref, true);
  server.ref();
  assert(server._handle.isRefed(), true);
  assert(server._unref, false);
  server.unref();
}


// timers
{
  const assert = makeAssert('isRefed() not working on timer_wrap');
  const timer = setTimeout(() => {}, 500);
  timer.unref();
  assert(Object.getPrototypeOf(timer._handle).hasOwnProperty('isRefed'), true);
  assert(timer._handle.isRefed(), false);
  timer.ref();
  assert(timer._handle.isRefed(), true);
  timer.unref();
}
