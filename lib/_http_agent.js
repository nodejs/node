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

var net = require('net');
var util = require('util');
var EventEmitter = require('events').EventEmitter;
var debug = util.debuglog('http');

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

  self.on('free', function(socket, options) {
    var name = self.getName(options);
    debug('agent.on(free)', name);

    if (!socket.destroyed &&
        self.requests[name] && self.requests[name].length) {
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
          !socket.destroyed &&
          self.options.keepAlive) {
        var freeSockets = self.freeSockets[name];
        var freeLen = freeSockets ? freeSockets.length : 0;
        var count = freeLen;
        if (self.sockets[name])
          count += self.sockets[name].length;

        if (count >= self.maxSockets || freeLen >= self.maxFreeSockets) {
          self.removeSocket(socket, options);
          socket.destroy();
        } else {
          freeSockets = freeSockets || [];
          self.freeSockets[name] = freeSockets;
          socket.setKeepAlive(true, self.keepAliveMsecs);
          socket.unref();
          socket._httpMessage = null;
          self.removeSocket(socket, options);
          freeSockets.push(socket);
        }
      } else {
        self.removeSocket(socket, options);
        socket.destroy();
      }
    }
  });
}

util.inherits(Agent, EventEmitter);
exports.Agent = Agent;

Agent.defaultMaxSockets = Infinity;

Agent.prototype.createConnection = net.createConnection;

// Get the key for a given set of request options
Agent.prototype.getName = function(options) {
  var name = '';

  if (options.host)
    name += options.host;
  else
    name += 'localhost';

  name += ':';
  if (options.port)
    name += options.port;
  name += ':';
  if (options.localAddress)
    name += options.localAddress;
  name += ':';
  return name;
};

Agent.prototype.addRequest = function(req, options) {
  // Legacy API: addRequest(req, host, port, path)
  if (typeof options === 'string') {
    options = {
      host: options,
      port: arguments[2],
      path: arguments[3]
    };
  }

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

    // don't leak
    if (!this.freeSockets[name].length)
      delete this.freeSockets[name];

    socket.ref();
    req.onSocket(socket);
    this.sockets[name].push(socket);
  } else if (sockLen < this.maxSockets) {
    debug('call onSocket', sockLen, freeLen);
    // If we are under maxSockets create a new one.
    req.onSocket(this.createSocket(req, options));
  } else {
    debug('wait for socket');
    // We are over limit so we'll add it to the queue.
    if (!this.requests[name]) {
      this.requests[name] = [];
    }
    this.requests[name].push(req);
  }
};

Agent.prototype.createSocket = function(req, options) {
  var self = this;
  options = util._extend({}, options);
  options = util._extend(options, self.options);

  options.servername = options.host;
  if (req) {
    var hostHeader = req.getHeader('host');
    if (hostHeader) {
      options.servername = hostHeader.replace(/:.*$/, '');
    }
  }

  var name = self.getName(options);

  debug('createConnection', name, options);
  options.encoding = null;
  var s = self.createConnection(options);
  if (!self.sockets[name]) {
    self.sockets[name] = [];
  }
  this.sockets[name].push(s);
  debug('sockets', name, this.sockets[name].length);

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
  }
  s.on('close', onClose);

  function onRemove() {
    // We need this function for cases like HTTP 'upgrade'
    // (defined by WebSockets) where we need to remove a socket from the
    // pool because it'll be locked up indefinitely
    debug('CLIENT socket onRemove');
    self.removeSocket(s, options);
    s.removeListener('close', onClose);
    s.removeListener('free', onFree);
    s.removeListener('agentRemove', onRemove);
  }
  s.on('agentRemove', onRemove);
  return s;
};

Agent.prototype.removeSocket = function(s, options) {
  var name = this.getName(options);
  debug('removeSocket', name, 'destroyed:', s.destroyed);
  var sets = [this.sockets];

  // If the socket was destroyed, remove it from the free buffers too.
  if (s.destroyed)
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

  if (this.requests[name] && this.requests[name].length) {
    debug('removeSocket, have a request, make a socket');
    var req = this.requests[name][0];
    // If we have pending requests and a socket gets closed make a new one
    this.createSocket(req, options).emit('free');
  }
};

Agent.prototype.destroy = function() {
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
