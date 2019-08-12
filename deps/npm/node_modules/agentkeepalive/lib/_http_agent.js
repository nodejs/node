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

// patch from https://github.com/nodejs/node/blob/v7.2.1/lib/_http_agent.js

'use strict';

const net = require('net');
const util = require('util');
const EventEmitter = require('events');
const debug = util.debuglog('http');

// New Agent code.

// The largest departure from the previous implementation is that
// an Agent instance holds connections for a variable number of host:ports.
// Surprisingly, this is still API compatible as far as third parties are
// concerned. The only code that really notices the difference is the
// request object.

// Another departure is that all code related to HTTP parsing is in
// ClientRequest.onSocket(). The Agent is now *strictly*
// concerned with managing a connection pool.

function Agent(options) {
  if (!(this instanceof Agent))
    return new Agent(options);

  EventEmitter.call(this);

  var self = this;

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

  // [patch start]
  // free keep-alive socket timeout. By default free socket do not have a timeout.
  self.freeSocketKeepAliveTimeout = self.options.freeSocketKeepAliveTimeout || 0;
  // working socket timeout. By default working socket do not have a timeout.
  self.timeout = self.options.timeout || 0;
  // the socket active time to live, even if it's in use
  this.socketActiveTTL = this.options.socketActiveTTL || null;
  // [patch end]

  self.on('free', function(socket, options) {
    var name = self.getName(options);
    debug('agent.on(free)', name);

    if (socket.writable &&
        self.requests[name] && self.requests[name].length) {
      // [patch start]
      debug('continue handle next request');
      // [patch end]
      self.requests[name].shift().onSocket(socket);
      if (self.requests[name].length === 0) {
        // don't leak
        delete self.requests[name];
      }
    } else {
      // If there are no pending requests, then put it in
      // the freeSockets pool, but only if we're allowed to do so.
      var req = socket._httpMessage;
      if (req &&
          req.shouldKeepAlive &&
          socket.writable &&
          self.keepAlive) {
        var freeSockets = self.freeSockets[name];
        var freeLen = freeSockets ? freeSockets.length : 0;
        var count = freeLen;
        if (self.sockets[name])
          count += self.sockets[name].length;

        if (count > self.maxSockets || freeLen >= self.maxFreeSockets) {
          socket.destroy();
        } else {
          freeSockets = freeSockets || [];
          self.freeSockets[name] = freeSockets;
          socket.setKeepAlive(true, self.keepAliveMsecs);
          socket.unref();
          socket._httpMessage = null;
          self.removeSocket(socket, options);
          freeSockets.push(socket);

          // [patch start]
          // Add a default error handler to avoid Unhandled 'error' event throw on idle socket
          // https://github.com/node-modules/agentkeepalive/issues/25
          // https://github.com/nodejs/node/pull/4482 (fixed in >= 4.4.0 and >= 5.4.0)
          if (socket.listeners('error').length === 0) {
            socket.once('error', freeSocketErrorListener);
          }
          // set free keepalive timer
          // try to use socket custom freeSocketKeepAliveTimeout first
          const freeSocketKeepAliveTimeout = socket.freeSocketKeepAliveTimeout || self.freeSocketKeepAliveTimeout;
          socket.setTimeout(freeSocketKeepAliveTimeout);
          debug(`push to free socket queue and wait for ${freeSocketKeepAliveTimeout}ms`);
          // [patch end]
        }
      } else {
        socket.destroy();
      }
    }
  });
}

util.inherits(Agent, EventEmitter);
exports.Agent = Agent;

// [patch start]
function freeSocketErrorListener(err) {
  var socket = this;
  debug('SOCKET ERROR on FREE socket:', err.message, err.stack);
  socket.destroy();
  socket.emit('agentRemove');
}
// [patch end]

Agent.defaultMaxSockets = Infinity;

Agent.prototype.createConnection = net.createConnection;

// Get the key for a given set of request options
Agent.prototype.getName = function getName(options) {
  var name = options.host || 'localhost';

  name += ':';
  if (options.port)
    name += options.port;

  name += ':';
  if (options.localAddress)
    name += options.localAddress;

  // Pacify parallel/test-http-agent-getname by only appending
  // the ':' when options.family is set.
  if (options.family === 4 || options.family === 6)
    name += ':' + options.family;

  return name;
};

// [patch start]
function handleSocketCreation(req) {
  return function(err, newSocket) {
    if (err) {
      process.nextTick(function() {
        req.emit('error', err);
      });
      return;
    }
    req.onSocket(newSocket);
  }
}
// [patch end]

Agent.prototype.addRequest = function addRequest(req, options, port/*legacy*/,
                                                 localAddress/*legacy*/) {
  // Legacy API: addRequest(req, host, port, localAddress)
  if (typeof options === 'string') {
    options = {
      host: options,
      port,
      localAddress
    };
  }

  options = util._extend({}, options);
  options = util._extend(options, this.options);

  if (!options.servername)
    options.servername = calculateServerName(options, req);

  var name = this.getName(options);
  if (!this.sockets[name]) {
    this.sockets[name] = [];
  }

  var freeLen = this.freeSockets[name] ? this.freeSockets[name].length : 0;
  var sockLen = freeLen + this.sockets[name].length;

  if (freeLen) {
    // we have a free socket, so use that.
    var socket = this.freeSockets[name].shift();
    debug('have free socket');

    // [patch start]
    // remove free socket error event handler
    socket.removeListener('error', freeSocketErrorListener);
    // restart the default timer
    socket.setTimeout(this.timeout);

    if (this.socketActiveTTL && Date.now() - socket.createdTime > this.socketActiveTTL) {
      debug(`socket ${socket.createdTime} expired`);
      socket.destroy();
      return this.createSocket(req, options, handleSocketCreation(req));
    }
    // [patch end]

    // don't leak
    if (!this.freeSockets[name].length)
      delete this.freeSockets[name];

    socket.ref();
    req.onSocket(socket);
    this.sockets[name].push(socket);
  } else if (sockLen < this.maxSockets) {
    debug('call onSocket', sockLen, freeLen);
    // If we are under maxSockets create a new one.
    // [patch start]
    this.createSocket(req, options, handleSocketCreation(req));
    // [patch end]
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
  var self = this;
  options = util._extend({}, options);
  options = util._extend(options, self.options);

  if (!options.servername)
    options.servername = calculateServerName(options, req);

  var name = self.getName(options);
  options._agentKey = name;

  debug('createConnection', name, options);
  options.encoding = null;
  var called = false;
  const newSocket = self.createConnection(options, oncreate);
  // [patch start]
  if (newSocket) {
    oncreate(null, Object.assign(newSocket, { createdTime: Date.now() }));
  }
  // [patch end]
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

    function onFree() {
      self.emit('free', s, options);
    }
    s.on('free', onFree);

    function onClose(err) {
      debug('CLIENT socket onClose');
      // This is the only place where sockets get removed from the Agent.
      // If you want to remove a socket from the pool, just close it.
      // All socket errors end in a close event anyway.
      self.removeSocket(s, options);

      // [patch start]
      self.emit('close');
      // [patch end]
    }
    s.on('close', onClose);

    // [patch start]
    // start socket timeout handler
    function onTimeout() {
      debug('CLIENT socket onTimeout');
      s.destroy();
      // Remove it from freeSockets immediately to prevent new requests from being sent through this socket.
      self.removeSocket(s, options);
      self.emit('timeout');
    }
    s.on('timeout', onTimeout);
    // set the default timer
    s.setTimeout(self.timeout);
    // [patch end]

    function onRemove() {
      // We need this function for cases like HTTP 'upgrade'
      // (defined by WebSockets) where we need to remove a socket from the
      // pool because it'll be locked up indefinitely
      debug('CLIENT socket onRemove');
      self.removeSocket(s, options);
      s.removeListener('close', onClose);
      s.removeListener('free', onFree);
      s.removeListener('agentRemove', onRemove);

      // [patch start]
      // remove socket timeout handler
      s.setTimeout(0, onTimeout);
      // [patch end]
    }
    s.on('agentRemove', onRemove);
    cb(null, s);
  }
};

function calculateServerName(options, req) {
  let servername = options.host;
  const hostHeader = req.getHeader('host');
  if (hostHeader) {
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
  return servername;
}

Agent.prototype.removeSocket = function removeSocket(s, options) {
  var name = this.getName(options);
  debug('removeSocket', name, 'writable:', s.writable);
  var sets = [this.sockets];

  // If the socket was destroyed, remove it from the free buffers too.
  if (!s.writable)
    sets.push(this.freeSockets);

  for (var sk = 0; sk < sets.length; sk++) {
    var sockets = sets[sk];

    if (sockets[name]) {
      var index = sockets[name].indexOf(s);
      if (index !== -1) {
        sockets[name].splice(index, 1);
        // Don't leak
        if (sockets[name].length === 0)
          delete sockets[name];
      }
    }
  }

  // [patch start]
  var freeLen = this.freeSockets[name] ? this.freeSockets[name].length : 0;
  var sockLen = freeLen + (this.sockets[name] ? this.sockets[name].length : 0);
  // [patch end]

  if (this.requests[name] && this.requests[name].length && sockLen < this.maxSockets) {
    debug('removeSocket, have a request, make a socket');
    var req = this.requests[name][0];
    // If we have pending requests and a socket gets closed make a new one
    this.createSocket(req, options, function(err, newSocket) {
      if (err) {
        process.nextTick(function() {
          req.emit('error', err);
        });
        return;
      }
      newSocket.emit('free');
    });
  }
};

Agent.prototype.destroy = function destroy() {
  var sets = [this.freeSockets, this.sockets];
  for (var s = 0; s < sets.length; s++) {
    var set = sets[s];
    var keys = Object.keys(set);
    for (var v = 0; v < keys.length; v++) {
      var setName = set[keys[v]];
      for (var n = 0; n < setName.length; n++) {
        setName[n].destroy();
      }
    }
  }
};

exports.globalAgent = new Agent();
