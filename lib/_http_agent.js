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
  EventEmitter.call(this);

  var self = this;
  self.options = options || {};
  self.requests = {};
  self.sockets = {};
  self.maxSockets = self.options.maxSockets || Agent.defaultMaxSockets;
  self.on('free', function(socket, host, port, localAddress) {
    var name = host + ':' + port;
    if (localAddress) {
      name += ':' + localAddress;
    }

    if (!socket.destroyed &&
        self.requests[name] && self.requests[name].length) {
      self.requests[name].shift().onSocket(socket);
      if (self.requests[name].length === 0) {
        // don't leak
        delete self.requests[name];
      }
    } else {
      // If there are no pending requests just destroy the
      // socket and it will get removed from the pool. This
      // gets us out of timeout issues and allows us to
      // default to Connection:keep-alive.
      socket.destroy();
    }
  });
  self.createConnection = net.createConnection;
}
util.inherits(Agent, EventEmitter);
exports.Agent = Agent;

Agent.defaultMaxSockets = 5;

Agent.prototype.defaultPort = 80;
Agent.prototype.addRequest = function(req, host, port, localAddress) {
  var name = host + ':' + port;
  if (localAddress) {
    name += ':' + localAddress;
  }
  if (!this.sockets[name]) {
    this.sockets[name] = [];
  }
  if (this.sockets[name].length < this.maxSockets) {
    // If we are under maxSockets create a new one.
    req.onSocket(this.createSocket(name, host, port, localAddress, req));
  } else {
    // We are over limit so we'll add it to the queue.
    if (!this.requests[name]) {
      this.requests[name] = [];
    }
    this.requests[name].push(req);
  }
};
Agent.prototype.createSocket = function(name, host, port, localAddress, req) {
  var self = this;
  var options = util._extend({}, self.options);
  options.port = port;
  options.host = host;
  options.localAddress = localAddress;

  options.servername = host;
  if (req) {
    var hostHeader = req.getHeader('host');
    if (hostHeader) {
      options.servername = hostHeader.replace(/:.*$/, '');
    }
  }

  var s = self.createConnection(options);
  if (!self.sockets[name]) {
    self.sockets[name] = [];
  }
  this.sockets[name].push(s);
  var onFree = function() {
    self.emit('free', s, host, port, localAddress);
  }
  s.on('free', onFree);
  var onClose = function(err) {
    // This is the only place where sockets get removed from the Agent.
    // If you want to remove a socket from the pool, just close it.
    // All socket errors end in a close event anyway.
    self.removeSocket(s, name, host, port, localAddress);
  }
  s.on('close', onClose);
  var onRemove = function() {
    // We need this function for cases like HTTP 'upgrade'
    // (defined by WebSockets) where we need to remove a socket from the pool
    //  because it'll be locked up indefinitely
    self.removeSocket(s, name, host, port, localAddress);
    s.removeListener('close', onClose);
    s.removeListener('free', onFree);
    s.removeListener('agentRemove', onRemove);
  }
  s.on('agentRemove', onRemove);
  return s;
};
Agent.prototype.removeSocket = function(s, name, host, port, localAddress) {
  if (this.sockets[name]) {
    var index = this.sockets[name].indexOf(s);
    if (index !== -1) {
      this.sockets[name].splice(index, 1);
      if (this.sockets[name].length === 0) {
        // don't leak
        delete this.sockets[name];
      }
    }
  }
  if (this.requests[name] && this.requests[name].length) {
    var req = this.requests[name][0];
    // If we have pending requests and a socket gets closed a new one
    this.createSocket(name, host, port, localAddress, req).emit('free');
  }
};

var globalAgent = new Agent();
exports.globalAgent = globalAgent;
