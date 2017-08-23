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

const net = require('net');
const util = require('util');
const EventEmitter = require('events');
const debug = util.debuglog('http');
const async_id_symbol = process.binding('async_wrap').async_id_symbol;
const nextTick = require('internal/process/next_tick').nextTick;

// New Agent code.

// The largest departure from the previous implementation is that
// an Agent instance holds connections for a variable number of host:ports.
// Surprisingly, this is still API compatible as far as third parties are
// concerned. The only code that really notices the difference is the
// request object.

// Another departure is that all code related to HTTP parsing is in
// ClientRequest.onSocket(). The Agent is now *strictly*
// concerned with managing a connection pool.

class Agent extends EventEmitter {
  constructor(options) {
    if (!(this instanceof Agent))
      return new Agent(options);

    super();

    const self = this;

    self.defaultPort = 80;
    self.protocol = 'http:';

    self.options = util._extend({}, options);

    // don't confuse net and make it think that we're connecting to a pipe
    self.options.path = null;
    self.requests = {};
    self.sockets = {};
    self.freeSockets = {};
    self.keepAliveMsecs = self.options.keepAliveMsecs || 1000;
    self.keepAlive = self.options.keepAlive || false;
    self.maxSockets = self.options.maxSockets || Agent.defaultMaxSockets;
    self.maxFreeSockets = self.options.maxFreeSockets || 256;

    self.on('free', (socket, options) => {
      const name = self.getName(options);
      debug('agent.on(free)', name);

      if (socket.writable &&
          self.requests[name] && self.requests[name].length) {
        self.requests[name].shift().onSocket(socket);
        if (self.requests[name].length === 0) {
          // don't leak
          delete self.requests[name];
        }
      } else {
        // If there are no pending requests, then put it in
        // the freeSockets pool, but only if we're allowed to do so.
        const req = socket._httpMessage;
        if (req &&
            req.shouldKeepAlive &&
            socket.writable &&
            self.keepAlive) {
          let freeSockets = self.freeSockets[name];
          const freeLen = freeSockets ? freeSockets.length : 0;
          let count = freeLen;
          if (self.sockets[name])
            count += self.sockets[name].length;

          if (count > self.maxSockets || freeLen >= self.maxFreeSockets) {
            socket.destroy();
          } else if (self.keepSocketAlive(socket)) {
            freeSockets = freeSockets || [];
            self.freeSockets[name] = freeSockets;
            socket[async_id_symbol] = -1;
            socket._httpMessage = null;
            self.removeSocket(socket, options);
            freeSockets.push(socket);
          } else {
            // Implementation doesn't want to keep socket alive
            socket.destroy();
          }
        } else {
          socket.destroy();
        }
      }
    });
  }

  // Get the key for a given set of request options
  getName(options) {
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

    return name;
  }

  addRequest(req, options, port/*legacy*/, localAddress/*legacy*/) {
    // Legacy API: addRequest(req, host, port, localAddress)
    if (typeof options === 'string') {
      options = {
        host: options,
        port,
        localAddress
      };
    }

    options = util._extend({}, options);
    util._extend(options, this.options);

    if (!options.servername) {
      options.servername = options.host;
      const hostHeader = req.getHeader('host');
      if (hostHeader) {
        options.servername = hostHeader.replace(/:.*$/, '');
      }
    }

    const name = this.getName(options);
    if (!this.sockets[name]) {
      this.sockets[name] = [];
    }

    const freeLen = this.freeSockets[name] ? this.freeSockets[name].length : 0;
    const sockLen = freeLen + this.sockets[name].length;

    if (freeLen) {
      // we have a free socket, so use that.
      const socket = this.freeSockets[name].shift();
      // Guard against an uninitialized or user supplied Socket.
      if (socket._handle && typeof socket._handle.asyncReset === 'function') {
        // Assign the handle a new asyncId and run any init() hooks.
        socket._handle.asyncReset();
        socket[async_id_symbol] = socket._handle.getAsyncId();
      }

      // don't leak
      if (!this.freeSockets[name].length)
        delete this.freeSockets[name];

      this.reuseSocket(socket, req);
      req.onSocket(socket);
      this.sockets[name].push(socket);
    } else if (sockLen < this.maxSockets) {
      debug('call onSocket', sockLen, freeLen);
      // If we are under maxSockets create a new one.
      this.createSocket(req, options, handleSocketCreation(req, true));
    } else {
      debug('wait for socket');
      // We are over limit so we'll add it to the queue.
      if (!this.requests[name]) {
        this.requests[name] = [];
      }
      this.requests[name].push(req);
    }
  }

  createSocket(req, options, cb) {
    const self = this;
    options = util._extend({}, options);
    util._extend(options, self.options);

    if (!options.servername) {
      options.servername = options.host;
      const hostHeader = req.getHeader('host');
      if (hostHeader) {
        options.servername = hostHeader.replace(/:.*$/, '');
      }
    }

    const name = self.getName(options);
    options._agentKey = name;

    debug('createConnection', name, options);
    options.encoding = null;
    let called = false;
    const newSocket = self.createConnection(options, oncreate);
    if (newSocket)
      oncreate(null, newSocket);

    function oncreate(err, s) {
      if (called)
        return;
      called = true;
      if (err)
        return cb(err);
      if (!self.sockets[name]) {
        self.sockets[name] = [];
      }
      self.sockets[name].push(s);
      debug('sockets', name, self.sockets[name].length);
      installListeners(self, s, options);
      cb(null, s);
    }
  }

  removeSocket(s, options) {
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
      this.createSocket(req, options, handleSocketCreation(req, false));
    }
  }

  keepSocketAlive(socket) {
    socket.setKeepAlive(true, this.keepAliveMsecs);
    socket.unref();

    return true;
  }

  reuseSocket(socket, req) {
    debug('have free socket');
    socket.ref();
  }

  destroy() {
    const sets = [this.freeSockets, this.sockets];

    for (const set of sets) {
      const keys = Object.keys(set);
      for (let v = 0; v < keys.length; v++) {
        const setName = set[keys[v]];
        for (let n = 0; n < setName.length; n++) {
          setName[n].destroy();
        }
      }
    }
  }
}

Agent.defaultMaxSockets = Infinity;

Agent.prototype.createConnection = net.createConnection;

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

  function onRemove() {
    // We need this function for cases like HTTP 'upgrade'
    // (defined by WebSockets) where we need to remove a socket from the
    // pool because it'll be locked up indefinitely
    debug('CLIENT socket onRemove');
    agent.removeSocket(s, options);
    s.removeListener('close', onClose);
    s.removeListener('free', onFree);
    s.removeListener('agentRemove', onRemove);
  }
  s.on('agentRemove', onRemove);
}

function handleSocketCreation(request, informRequest) {
  return function handleSocketCreation_Inner(err, socket) {
    if (err) {
      const asyncId = (socket && socket._handle && socket._handle.getAsyncId) ?
        socket._handle.getAsyncId() :
        null;
      nextTick(asyncId, () => request.emit('error', err));
      return;
    }
    if (informRequest)
      request.onSocket(socket);
    else
      socket.emit('free');
  };
}

module.exports = {
  Agent,
  globalAgent: new Agent()
};
