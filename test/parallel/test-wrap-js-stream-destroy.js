// Flags: --expose-internals
'use strict';

const common = require('../common');
const StreamWrap = require('internal/js_stream_socket');
const net = require('net');

// This test ensures that when we directly call `socket.destroy()` without
// having called `socket.end()` on an instance of streamWrap, it will
// still emit EOF which makes the socket on the other side emit "end" and
// "close" events, and vice versa.
{
  let port;
  const server = net.createServer((socket) => {
    socket.on('error', common.mustNotCall());
    socket.on('end', common.mustNotCall());
    socket.on('close', common.mustCall());
    socket.destroy();
  });

  server.listen(() => {
    port = server.address().port;
    createSocket();
  });

  function createSocket() {
    let streamWrap;
    const socket = new net.connect({
      port,
    }, () => {
      socket.on('error', common.mustNotCall());
      socket.on('end', common.mustCall());
      socket.on('close', common.mustCall());

      streamWrap.on('error', common.mustNotCall());
      // The "end" events will be emitted which is as same as
      // the same situation for an instance of `net.Socket` without
      // `StreamWrap`.
      streamWrap.on('end', common.mustCall());
      // Destroying a socket in the server side should emit EOF and cause
      // the corresponding client-side socket closed.
      streamWrap.on('close', common.mustCall(() => {
        server.close();
      }));
    });
    streamWrap = new StreamWrap(socket);
  }
}

// Destroy the streamWrap and test again.
{
  let port;
  const server = net.createServer((socket) => {
    socket.on('error', common.mustNotCall());
    socket.on('end', common.mustCall());
    socket.on('close', common.mustCall(() => {
      server.close();
    }));
    // Do not `socket.end()` and directly `socket.destroy()`.
  });

  server.listen(() => {
    port = server.address().port;
    createSocket();
  });

  function createSocket() {
    let streamWrap;
    const socket = new net.connect({
      port,
    }, () => {
      socket.on('error', common.mustNotCall());
      socket.on('end', common.mustNotCall());
      socket.on('close', common.mustCall());

      streamWrap.on('error', common.mustNotCall());
      streamWrap.on('end', common.mustNotCall());
      // Destroying a socket in the server side should emit EOF and cause
      // the corresponding client-side socket closed.
      streamWrap.on('close', common.mustCall());
      streamWrap.destroy();
    });
    streamWrap = new StreamWrap(socket);
  }
}

// Destroy the client socket and test again.
{
  let port;
  const server = net.createServer((socket) => {
    socket.on('error', common.mustNotCall());
    socket.on('end', common.mustCall());
    socket.on('close', common.mustCall(() => {
      server.close();
    }));
  });

  server.listen(() => {
    port = server.address().port;
    createSocket();
  });

  function createSocket() {
    let streamWrap;
    const socket = new net.connect({
      port,
    }, () => {
      socket.on('error', common.mustNotCall());
      socket.on('end', common.mustNotCall());
      socket.on('close', common.mustCall());

      streamWrap.on('error', common.mustNotCall());
      streamWrap.on('end', common.mustNotCall());
      streamWrap.on('close', common.mustCall());
      socket.destroy();
    });
    streamWrap = new StreamWrap(socket);
  }
}
