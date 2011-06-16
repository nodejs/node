var events = require('events');
var stream = require('stream');
var timers = require('timers');
var util = require('util');
var assert = require('assert');
var TCP = process.binding('tcp_wrap').TCP;


var debug;
if (process.env.NODE_DEBUG && /net/.test(process.env.NODE_DEBUG)) {
  debug = function(x) { console.error('NET:', x); };
} else {
  debug = function() { };
}


function Socket(options) {
  if (!(this instanceof Socket)) return new Socket(options);

  stream.Stream.call(this);

  // private
  if (options.handle) {
    this._handle = options.handle;
  } else {
    this._handle = new TCP();
  }
  this._handle.socket = this;
  this._handle.onread = onread;

  this._writeRequests = [];
}
util.inherits(Socket, stream.Stream);


Socket.prototype.setTimeout = function(msecs, callback) {
  if (msecs > 0) {
    timers.enroll(this, msecs);
    if (typeof this.fd === 'number') { timers.active(this); }
    if (callback) {
      this.once('timeout', callback);
    }
  } else if (msecs === 0) {
    timers.unenroll(this);
  }
};


Socket.prototype.pause = function() {
  this._handle.readStop();
};


Socket.prototype.resume = function() {
  this._handle.readStart();
};


Socket.prototype.end = function() {
  throw new Error("implement me");
};


Socket.prototype.destroySoon = function() {
  throw new Error("implement me");
};


Socket.prototype.destroy = function(exception) {
  var self = this;

  debug('destroy ' + this.fd);

  this.readable = this.writable = false;

  timers.unenroll(this);

  if (this.server && !this.destroyed) {
    this.server.connections--;
  }

  debug('close ' + this.fd);
  this._handle.close();

  process.nextTick(function() {
    if (exception) self.emit('error', exception);
    self.emit('close', exception ? true : false);
  });

  this.destroyed = true;
};


function onread(buffer, offset, length) {
  var handle = this;
  var self = handle.socket;
  assert.equal(handle, self._handle);

  timers.active(self);

  var end = offset + length;

  // Emit 'data' event.

  if (self._decoder) {
    // Emit a string.
    var string = self._decoder.write(buffer.slice(offset, end));
    if (string.length) self.emit('data', string);
  } else {
    // Emit a slice. Attempt to avoid slicing the buffer if no one is
    // listening for 'data'.
    if (self._events && self._events['data']) {
      self.emit('data', buffer.slice(offset, end));
    }
  }

  // Optimization: emit the original buffer with end points
  if (self.ondata) self.ondata(buffer, offset, end);
}


Socket.prototype.setEncoding = function(encoding) {
  var StringDecoder = require('string_decoder').StringDecoder; // lazy load
  this._decoder = new StringDecoder(encoding);
};


Socket.prototype.write = function(data /* [encoding], [fd], [cb] */) {
  var encoding, fd, cb;

  // parse arguments
  if (typeof arguments[1] == 'string') {
    encoding = arguments[1];
    if (typeof arguments[2] == 'number') {
      fd = arguments[2];
      cb = arguments[3];
    } else {
      cb = arguments[2];
    }
  } else if (typeof arguments[1] == 'number') {
    fd = arguments[1];
    cb = arguments[2];
  } else if (typeof arguments[2] == 'number') {
    // This case is to support old calls when the encoding argument
    // was not optional: s.write(buf, undefined, pipeFDs[1])
    encoding = arguments[1];
    fd = arguments[2];
    cb = arguments[3];
  } else {
    cb = arguments[1];
  }

  // Change strings to buffers. SLOW
  if (typeof data == 'string') {
    data = new Buffer(data, encoding);
  }

  var writeReq = this._handle.write(data);
  writeReq.oncomplete = afterWrite;
  writeReq.cb = cb;
  this._writeRequests.push(writeReq);

  return this._handle.writeQueueSize == 0;
};


function afterWrite(status, handle, req, buffer) {
  var self = handle.socket;

  // TODO check status.

  var req_ = self._writeRequests.shift();
  assert.equal(req, req_);

  if (req.cb) req.cb();
}


Socket.prototype.connect = function(port, host) {
  var self = this;

  timers.active(this);

  require('dns').lookup(host, function(err, ip, addressType) {
    if (err) {
      self.emit('error', err);
    } else {
      timers.active(self);

      if (addressType != 4) {
        throw new Error("ipv6 addresses not yet supported by libuv");
      }

      self.remoteAddress = ip;
      self.remotePort = port;

      // TODO retrun promise from Socket.prototype.connect which
      // wraps _connectReq.

      assert.ok(!self._connectReq);
 
      self._connectReq = self._handle.connect(ip, port);

      if (!self._connectReq) {
        self.destroy(errnoException(errno, 'connect'));
      }

      self._connectReq.oncomplete = afterConnect;
    }
  });
};


function afterConnect(status, handle, req) {
  var self = handle.socket;
  assert.equal(handle, self._handle);

  if (status == 0) {
    self.readable = self.writable = true;
    timers.active(self);
    handle.readStart();
    self.emit('connect');
  } else {
    self.destroy(errnoException(errno, 'connect'));
  }
}


function errnoException(errorno, syscall) {
  // TODO make this more compatible with ErrnoException from src/node.cc
  // Once all of Node is using this function the ErrnoException from
  // src/node.cc should be removed.
  var e = new Error(syscall + ' ' + errorno);
  e.errno = errorno;
  e.syscall = syscall;
  return e;
}




function Server(/* [ options, ] listener */) {
  if (!(this instanceof Server)) return new Server(arguments[0], arguments[1]);
  events.EventEmitter.call(this);

  var self = this;

  var options, listenerCallback;

  if (typeof arguments[0] == 'function') {
    options = {};
    listenerCallback = arguments[0];
  } else {
    options = arguments[0];
    
    if (typeof arguments[1] == 'function') {
      listenerCallback = arguments[1];
    }
  }

  this.on('connection', listenerCallback);
  this.connections = 0;
  self.allowHalfOpen = options.allowHalfOpen || false;


  this._handle = new TCP();
  this._handle.socket = this;
  this._handle.onconnection = onconnection;
}
util.inherits(Server, events.EventEmitter);
exports.Server = Server;


function toPort(x) { return (x = Number(x)) >= 0 ? x : false; }


Server.prototype.listen = function() {
  var self = this;

  var lastArg = arguments[arguments.length - 1];
  if (typeof lastArg == 'function') {
    self.addListener('listening', lastArg);
  }

  var port = toPort(arguments[0]);

  if (arguments.length == 0 || typeof arguments[0] == 'function') {
    // Don't bind(). OS will assign a port with INADDR_ANY.
    // The port can be found with server.address()
    this._handle.listen(self._backlog || 128);
    this.emit('listening');
  } else {
    // the first argument is the port, the second an IP
    require('dns').lookup(arguments[1], function(err, ip, addressType) {
      if (err) {
        self.emit('error', err);
      } else {
        if (addressType != 4) {
          throw new Error("ipv6 addresses not yet supported by libuv");
        }

        var r = self._handle.bind(ip || '0.0.0.0', port);
        if (r) {
          self.emit('error', errnoException(errno, 'listen'));
        } else {
          self._handle.listen(self._backlog || 128);
          self.emit('listening');
        }
      }
    });
  }
};


function onconnection(clientHandle) {
  var handle = this;
  var self = handle.socket;

  var socket = new Socket({ handle: clientHandle });
  socket.resume();

  DTRACE_NET_SERVER_CONNECTION(socket);
  self.emit('connection', socket);
}

