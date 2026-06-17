'use strict';

const { once } = require('events');
const {
  validateAbortSignal,
  validateObject,
} = require('internal/validators');
const { AbortError } = require('internal/errors');
const { kEmptyObject } = require('internal/util');

// Lazily loaded to avoid a require cycle with the `net` module, which exposes
// this namespace through its `promises` getter.
let net;
function lazyNet() {
  net ??= require('net');
  return net;
}

// Resolves with a connected `net.Socket` once the `'connect'` event fires, and
// rejects if the connection fails or the optional `signal` is aborted.
async function connect(...args) {
  const lazy = lazyNet();
  const options = lazy._normalizeArgs(args)[0];
  const { signal } = options;
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
    if (signal.aborted) {
      throw new AbortError(undefined, { cause: signal.reason });
    }
  }

  // Strip the signal so the socket does not also install its own abort
  // handling; rejecting and destroying below fully tears the socket down.
  const socket = lazy.connect({ ...options, signal: undefined });

  try {
    await once(socket, 'connect', signal !== undefined ? { signal } : kEmptyObject);
  } catch (err) {
    socket.destroy();
    throw err;
  }
  return socket;
}

// Creates a server and resolves with it once it is listening, rejecting if it
// fails to bind or the optional `signal` is aborted.
async function listen(options = kEmptyObject) {
  validateObject(options, 'options');
  const { signal, connectionListener } = options;
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
    if (signal.aborted) {
      throw new AbortError(undefined, { cause: signal.reason });
    }
  }

  const lazy = lazyNet();
  // Strip the signal so listen() does not install its own close-on-abort
  // handler; rejecting and closing below tears the server down.
  const serverOptions = { ...options, signal: undefined };
  const server = lazy.createServer(serverOptions, connectionListener);

  try {
    server.listen(serverOptions);
    await once(server, 'listening', signal !== undefined ? { signal } : kEmptyObject);
  } catch (err) {
    server.close();
    throw err;
  }
  return server;
}

module.exports = {
  connect,
  listen,
  get isIP() { return lazyNet().isIP; },
  get isIPv4() { return lazyNet().isIPv4; },
  get isIPv6() { return lazyNet().isIPv6; },
  get BlockList() { return lazyNet().BlockList; },
  get SocketAddress() { return lazyNet().SocketAddress; },
};
