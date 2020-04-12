// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ObjectKeys,
  ObjectSetPrototypeOf,
  ObjectValues,
  Symbol,
} = primordials;

const net = require('net');
const EventEmitter = require('events');
const debug = require('internal/util/debuglog').debuglog('http');
const { async_id_symbol } = require('internal/async_hooks').symbols;
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const { once } = require('internal/util');

const kOnKeylog = Symbol('onkeylog');
// New Agent code.

// The largest departure from the previous implementation is that
// an Agent instance holds connections for a variable number of host:ports.
// Surprisingly, this is still API compatible as far as third parties are
// concerned. The only code that really notices the difference is the
// request object.

// Another departure is that all code related to HTTP parsing is in
// ClientRequest.onSocket(). The Agent is now *strictly*
// concerned with managing a connection pool.

class ReusedHandle {
  constructor(type, handle) {
    this.type = type;
    this.handle = handle;
  }
}

function freeSocketErrorListener(err) {
  const socket = this;
  debug('SOCKET ERROR on FREE socket:', err.message, err.stack);
  socket.destroy();
  socket.emit('agentRemove');
}

function Agent(options) {
  if (!(this instanceof Agent))
    return new Agent(options);

  EventEmitter.call(this);

  this.defaultPort = 80;
  this.protocol = 'http:';

  this.options = { ...options };

  // Don't confuse net and make it think that we're connecting to a pipe
  this.options.path = null;
  this.requests = {};
  this.sockets = {};
  this.freeSockets = {};
  this.keepAliveMsecs = this.options.keepAliveMsecs || 1000;
  this.keepAlive = this.options.keepAlive || false;
  this.maxSockets = this.options.maxSockets || Agent.defaultMaxSockets;
  this.maxFreeSockets = this.options.maxFreeSockets || 256;

  this.on('free', (socket, options) => {
    const name = this.getName(options);
    debug('agent.on(free)', name);

    // TODO(ronag): socket.destroy(err) might have been called
    // before coming here and have an 'error' scheduled. In the
    // case of socket.destroy() below this 'error' has no handler
    // and could cause unhandled exception.

    if (!socket.writable) {
      socket.destroy();
      return;
    }

    const requests = this.requests[name];
    if (requests && requests.length) {
      const req = requests.shift();
      setRequestSocket(this, req, socket);
      if (requests.length === 0) {
        delete this.requests[name];
      }
      return;
    }

    // If there are no pending requests, then put it in
    // the freeSockets pool, but only if we're allowed to do so.
    const req = socket._httpMessage;
    if (!req || !req.shouldKeepAlive || !this.keepAlive) {
      socket.destroy();
      return;
    }

    let freeSockets = this.freeSockets[name];
    const freeLen = freeSockets ? freeSockets.length : 0;
    let count = freeLen;
    if (this.sockets[name])
      count += this.sockets[name].length;

    if (count > this.maxSockets ||
        freeLen >= this.maxFreeSockets ||
        !this.keepSocketAlive(socket)) {
      socket.destroy();
      return;
    }

    freeSockets = freeSockets || [];
    this.freeSockets[name] = freeSockets;
    socket[async_id_symbol] = -1;
    socket._httpMessage = null;
    this.removeSocket(socket, options);

    const agentTimeout = this.options.timeout || 0;
    if (socket.timeout !== agentTimeout) {
      socket.setTimeout(agentTimeout);
    }

    socket.once('error', freeSocketErrorListener);
    freeSockets.push(socket);
  });

  // Don't emit keylog events unless there is a listener for them.
  this.on('newListener', maybeEnableKeylog);
}
ObjectSetPrototypeOf(Agent.prototype, EventEmitter.prototype);
ObjectSetPrototypeOf(Agent, EventEmitter);

function maybeEnableKeylog(eventName) {
  if (eventName === 'keylog') {
    this.removeListener('newListener', maybeEnableKeylog);
    // Future sockets will listen on keylog at creation.
    const agent = this;
    this[kOnKeylog] = function onkeylog(keylog) {
      agent.emit('keylog', keylog, this);
    };
    // Existing sockets will start listening on keylog now.
    for (const socket of ObjectValues(this.sockets)) {
      socket.on('keylog', this[kOnKeylog]);
    }
  }
}

Agent.defaultMaxSockets = Infinity;

Agent.prototype.createConnection = net.createConnection;

// Get the key for a given set of request options
Agent.prototype.getName = function getName(options) {
  let name = options.host || 'localhost';

  name += ':';
  if (options.port)
    name += options.port;

  name += ':';
  if (options.localAddress)
    name += options.localAddress;

  // Pacify parallel/test-http-agent-getname by only appending
  // the ':' when options.family is set.
  if (options.family === 4 || options.family === 6)
    name += `:${options.family}`;

  if (options.socketPath)
    name += `:${options.socketPath}`;

  return name;
};

Agent.prototype.addRequest = function addRequest(req, options, port/* legacy */,
                                                 localAddress/* legacy */) {
  // Legacy API: addRequest(req, host, port, localAddress)
  if (typeof options === 'string') {
    options = {
      host: options,
      port,
      localAddress
    };
  }

  options = { ...options, ...this.options };
  if (options.socketPath)
    options.path = options.socketPath;

  if (!options.servername && options.servername !== '')
    options.servername = calculateServerName(options, req);

  const name = this.getName(options);
  if (!this.sockets[name]) {
    this.sockets[name] = [];
  }

  const freeSockets = this.freeSockets[name];
  let socket;
  if (freeSockets) {
    while (freeSockets.length && freeSockets[0].destroyed) {
      freeSockets.shift();
    }
    socket = freeSockets.shift();
    if (!freeSockets.length)
      delete this.freeSockets[name];
  }

  const freeLen = freeSockets ? freeSockets.length : 0;
  const sockLen = freeLen + this.sockets[name].length;

  if (socket) {
    // Guard against an uninitialized or user supplied Socket.
    const handle = socket._handle;
    if (handle && typeof handle.asyncReset === 'function') {
      // Assign the handle a new asyncId and run any destroy()/init() hooks.
      handle.asyncReset(new ReusedHandle(handle.getProviderType(), handle));
      socket[async_id_symbol] = handle.getAsyncId();
    }

    this.reuseSocket(socket, req);
    setRequestSocket(this, req, socket);
    this.sockets[name].push(socket);
  } else if (sockLen < this.maxSockets) {
    debug('call onSocket', sockLen, freeLen);
    // If we are under maxSockets create a new one.
    this.createSocket(req, options, handleSocketCreation(this, req, true));
  } else {
    debug('wait for socket');
    // We are over limit so we'll add it to the queue.
    if (!this.requests[name]) {
      this.requests[name] = [];
    }
    this.requests[name].push(req);
  }
};

Agent.prototype.createSocket = function createSocket(req, options, cb) {
  options = { ...options, ...this.options };
  if (options.socketPath)
    options.path = options.socketPath;

  if (!options.servername && options.servername !== '')
    options.servername = calculateServerName(options, req);

  const name = this.getName(options);
  options._agentKey = name;

  debug('createConnection', name, options);
  options.encoding = null;

  const oncreate = once((err, s) => {
    if (err)
      return cb(err);
    if (!this.sockets[name]) {
      this.sockets[name] = [];
    }
    this.sockets[name].push(s);
    debug('sockets', name, this.sockets[name].length);
    installListeners(this, s, options);
    cb(null, s);
  });

  const newSocket = this.createConnection(options, oncreate);
  if (newSocket)
    oncreate(null, newSocket);
};

function calculateServerName(options, req) {
  let servername = options.host;
  const hostHeader = req.getHeader('host');
  if (hostHeader) {
    if (typeof hostHeader !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('options.headers.host',
                                     'String', hostHeader);
    }

    // abc => abc
    // abc:123 => abc
    // [::1] => ::1
    // [::1]:123 => ::1
    if (hostHeader.startsWith('[')) {
      const index = hostHeader.indexOf(']');
      if (index === -1) {
        // Leading '[', but no ']'. Need to do something...
        servername = hostHeader;
      } else {
        servername = hostHeader.substr(1, index - 1);
      }
    } else {
      servername = hostHeader.split(':', 1)[0];
    }
  }
  // Don't implicitly set invalid (IP) servernames.
  if (net.isIP(servername))
    servername = '';
  return servername;
}

function installListeners(agent, s, options) {
  function onFree() {
    debug('CLIENT socket onFree');
    agent.emit('free', s, options);
  }
  s.on('free', onFree);

  function onClose(err) {
    debug('CLIENT socket onClose');
    // This is the only place where sockets get removed from the Agent.
    // If you want to remove a socket from the pool, just close it.
    // All socket errors end in a close event anyway.
    agent.removeSocket(s, options);
  }
  s.on('close', onClose);

  function onTimeout() {
    debug('CLIENT socket onTimeout');

    // Destroy if in free list.
    // TODO(ronag): Always destroy, even if not in free list.
    const sockets = agent.freeSockets;
    for (const name of ObjectKeys(sockets)) {
      if (sockets[name].includes(s)) {
        return s.destroy();
      }
    }
  }
  s.on('timeout', onTimeout);

  function onRemove() {
    // We need this function for cases like HTTP 'upgrade'
    // (defined by WebSockets) where we need to remove a socket from the
    // pool because it'll be locked up indefinitely
    debug('CLIENT socket onRemove');
    agent.removeSocket(s, options);
    s.removeListener('close', onClose);
    s.removeListener('free', onFree);
    s.removeListener('timeout', onTimeout);
    s.removeListener('agentRemove', onRemove);
  }
  s.on('agentRemove', onRemove);

  if (agent[kOnKeylog]) {
    s.on('keylog', agent[kOnKeylog]);
  }
}

Agent.prototype.removeSocket = function removeSocket(s, options) {
  const name = this.getName(options);
  debug('removeSocket', name, 'writable:', s.writable);
  const sets = [this.sockets];

  // If the socket was destroyed, remove it from the free buffers too.
  if (!s.writable)
    sets.push(this.freeSockets);

  for (const sockets of sets) {
    if (sockets[name]) {
      const index = sockets[name].indexOf(s);
      if (index !== -1) {
        sockets[name].splice(index, 1);
        // Don't leak
        if (sockets[name].length === 0)
          delete sockets[name];
      }
    }
  }

  if (this.requests[name] && this.requests[name].length) {
    debug('removeSocket, have a request, make a socket');
    const req = this.requests[name][0];
    // If we have pending requests and a socket gets closed make a new one
    const socketCreationHandler = handleSocketCreation(this, req, false);
    this.createSocket(req, options, socketCreationHandler);
  }
};

Agent.prototype.keepSocketAlive = function keepSocketAlive(socket) {
  socket.setKeepAlive(true, this.keepAliveMsecs);
  socket.unref();

  return true;
};

Agent.prototype.reuseSocket = function reuseSocket(socket, req) {
  debug('have free socket');
  socket.removeListener('error', freeSocketErrorListener);
  req.reusedSocket = true;
  socket.ref();
};

Agent.prototype.destroy = function destroy() {
  for (const set of [this.freeSockets, this.sockets]) {
    for (const key of ObjectKeys(set)) {
      for (const setName of set[key]) {
        setName.destroy();
      }
    }
  }
};

function handleSocketCreation(agent, request, informRequest) {
  return function handleSocketCreation_Inner(err, socket) {
    if (err) {
      process.nextTick(emitErrorNT, request, err);
      return;
    }
    if (informRequest)
      setRequestSocket(agent, request, socket);
    else
      socket.emit('free');
  };
}

function setRequestSocket(agent, req, socket) {
  req.onSocket(socket);
  const agentTimeout = agent.options.timeout || 0;
  if (req.timeout === undefined || req.timeout === agentTimeout) {
    return;
  }
  socket.setTimeout(req.timeout);
}

function emitErrorNT(emitter, err) {
  emitter.emit('error', err);
}

module.exports = {
  Agent,
  globalAgent: new Agent()
};
