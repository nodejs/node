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

var EventEmitter = require('events').EventEmitter;
var net = require('net');
var Process = process.binding('process_wrap').Process;
var util = require('util');
var constants; // if (!constants) constants = process.binding('constants');

var Pipe;


// constructors for lazy loading
function createPipe(ipc) {
  // Lazy load
  if (!Pipe) {
    Pipe = process.binding('pipe_wrap').Pipe;
  }

  return new Pipe(ipc);
}

function createSocket(pipe, readable) {
  var s = new net.Socket({ handle: pipe });

  if (readable) {
    s.writable = false;
    s.readable = true;
    s.resume();
  } else {
    s.writable = true;
    s.readable = false;
  }

  return s;
}


// this object contain function to convert TCP objects to native handle objects
// and back again.
var handleConversion = {
  'net.Native': {
    simultaneousAccepts: true,

    send: function(message, handle) {
      return handle;
    },

    got: function(message, handle, emit) {
      emit(handle);
    }
  },

  'net.Server': {
    simultaneousAccepts: true,

    send: function(message, server) {
      return server._handle;
    },

    got: function(message, handle, emit) {
      var self = this;

      var server = new net.Server();
      server.listen(handle, function() {
        emit(server);
      });
    }
  },

  'net.Socket': {
    send: function(message, socket) {
      // pause socket so no data is lost, will be resumed later
      socket.pause();

      // if the socket wsa created by net.Server
      if (socket.server) {
        // the slave should keep track of the socket
        message.key = socket.server._connectionKey;

        var firstTime = !this._channel.sockets.send[message.key];

        // add socket to connections list
        var socketList = getSocketList('send', this, message.key);
        socketList.add(socket);

        // the server should no longer expose a .connection property
        // and when asked to close it should query the socket status from slaves
        if (firstTime) {
          socket.server._setupSlave(socketList);
        }
      }

      // remove handle from socket object, it will be closed when the socket
      // has been send
      var handle = socket._handle;
      handle.onread = function() {};
      socket._handle = null;

      return handle;
    },

    got: function(message, handle, emit) {
      var socket = new net.Socket({handle: handle});
      socket.readable = socket.writable = true;
      socket.pause();

      // if the socket was created by net.Server we will track the socket
      if (message.key) {

        // add socket to connections list
        var socketList = getSocketList('got', this, message.key);
        socketList.add(socket);
      }

      emit(socket);
      socket.resume();
    }
  }
};

// This object keep track of the socket there are sended
function SocketListSend(slave, key) {
  var self = this;

  this.key = key;
  this.list = [];
  this.slave = slave;

  slave.once('disconnect', function() {
    self.flush();
  });

  this.slave.on('internalMessage', function(msg) {
    if (msg.cmd !== 'NODE_SOCKET_CLOSED' || msg.key !== self.key) return;
    self.flush();
  });
}
util.inherits(SocketListSend, EventEmitter);

SocketListSend.prototype.add = function(socket) {
  this.list.push(socket);
};

SocketListSend.prototype.flush = function() {
  var list = this.list;
  this.list = [];

  list.forEach(function(socket) {
    socket.destroy();
  });
};

SocketListSend.prototype.update = function() {
  if (this.slave.connected === false) return;

  this.slave.send({
    cmd: 'NODE_SOCKET_FETCH',
    key: this.key
  });
};

// This object keep track of the socket there are received
function SocketListReceive(slave, key) {
  var self = this;

  this.key = key;
  this.list = [];
  this.slave = slave;

  slave.on('internalMessage', function(msg) {
    if (msg.cmd !== 'NODE_SOCKET_FETCH' || msg.key !== self.key) return;

    if (self.list.length === 0) {
      self.flush();
      return;
    }

    self.on('itemRemoved', function removeMe() {
      if (self.list.length !== 0) return;
      self.removeListener('itemRemoved', removeMe);
      self.flush();
    });
  });
}
util.inherits(SocketListReceive, EventEmitter);

SocketListReceive.prototype.flush = function() {
  this.list = [];

  if (this.slave.connected) {
    this.slave.send({
      cmd: 'NODE_SOCKET_CLOSED',
      key: this.key
    });
  }
};

SocketListReceive.prototype.add = function(socket) {
  var self = this;
  this.list.push(socket);

  socket.on('close', function() {
    self.list.splice(self.list.indexOf(socket), 1);
    self.emit('itemRemoved');
  });
};

function getSocketList(type, slave, key) {
  var sockets = slave._channel.sockets[type];
  var socketList = sockets[key];
  if (!socketList) {
    var Construct = type === 'send' ? SocketListSend : SocketListReceive;
    socketList = sockets[key] = new Construct(slave, key);
  }
  return socketList;
}

function handleMessage(target, message, handle) {
  //Filter out internal messages
  //if cmd property begin with "_NODE"
  if (message !== null &&
      typeof message === 'object' &&
      typeof message.cmd === 'string' &&
      message.cmd.indexOf('NODE_') === 0) {
    target.emit('internalMessage', message, handle);
  }
  //Non-internal message
  else {
    target.emit('message', message, handle);
  }
}

function setupChannel(target, channel) {
  target._channel = channel;

  var jsonBuffer = '';
  channel.buffering = false;
  channel.onread = function(pool, offset, length, recvHandle) {
    if (pool) {
      jsonBuffer += pool.toString('ascii', offset, offset + length);

      var i, start = 0;

      //Linebreak is used as a message end sign
      while ((i = jsonBuffer.indexOf('\n', start)) >= 0) {
        var json = jsonBuffer.slice(start, i);
        var message = JSON.parse(json);

        handleMessage(target, message, recvHandle);

        start = i + 1;
      }
      jsonBuffer = jsonBuffer.slice(start);
      this.buffering = jsonBuffer.length !== 0;

    } else {
      this.buffering = false;
      target.disconnect();
    }
  };

  // object where socket lists will live
  channel.sockets = { got: {}, send: {} };

  // handlers will go through this
  target.on('internalMessage', function(message, handle) {
    if (message.cmd !== 'NODE_HANDLE') return;

    var obj = handleConversion[message.type];

    // Update simultaneous accepts on Windows
    if (obj.simultaneousAccepts) {
      net._setSimultaneousAccepts(handle);
    }

    // Convert handle object
    obj.got.call(this, message, handle, function(handle) {
      handleMessage(target, message.msg, handle);
    });
  });

  target.send = function(message, handle) {
    if (typeof message === 'undefined') {
      throw new TypeError('message cannot be undefined');
    }

    if (!this.connected) {
      this.emit('error', new Error('channel closed'));
      return;
    }

    // For overflow protection don't write if channel queue is too deep.
    if (channel.writeQueueSize > 1024 * 1024) {
      return false;
    }

    // package messages with a handle object
    if (handle) {
      // this message will be handled by an internalMessage event handler
      message = {
        cmd: 'NODE_HANDLE',
        type: 'net.',
        msg: message
      };

      switch (handle.constructor.name) {
        case 'Socket':
          message.type += 'Socket'; break;
        case 'Server':
          message.type += 'Server'; break;
        case 'Pipe':
        case 'TCP':
          message.type += 'Native'; break;
      }

      var obj = handleConversion[message.type];

      // convert TCP object to native handle object
      handle = handleConversion[message.type].send.apply(target, arguments);

      // Update simultaneous accepts on Windows
      if (obj.simultaneousAccepts) {
        net._setSimultaneousAccepts(handle);
      }
    }

    var string = JSON.stringify(message) + '\n';
    var writeReq = channel.writeUtf8String(string, handle);

    // Close the Socket handle after sending it
    if (message && message.type === 'net.Socket') {
      handle.close();
    }

    if (!writeReq) {
      var er = errnoException(errno, 'write', 'cannot write to IPC channel.');
      this.emit('error', er);
    }

    writeReq.oncomplete = nop;

    return true;
  };

  target.connected = true;
  target.disconnect = function() {
    if (!this.connected) {
      this.emit('error', new Error('IPC channel is already disconnected'));
      return;
    }

    // do not allow messages to be written
    this.connected = false;
    this._channel = null;

    var fired = false;
    function finish() {
      if (fired) return;
      fired = true;

      channel.close();
      target.emit('disconnect');
    }

    // If a message is being read, then wait for it to complete.
    if (channel.buffering) {
      this.once('message', finish);
      this.once('internalMessage', finish);

      return;
    }

    finish();
  };

  channel.readStart();
}


function nop() { }

exports.fork = function(modulePath /*, args, options*/) {

  // Get options and args arguments.
  var options, args, execArgv;
  if (Array.isArray(arguments[1])) {
    args = arguments[1];
    options = util._extend({}, arguments[2]);
  } else {
    args = [];
    options = util._extend({}, arguments[1]);
  }

  // Prepare arguments for fork:
  execArgv = options.execArgv || process.execArgv;
  args = execArgv.concat([modulePath], args);

  // Don't allow stdinStream and customFds since a stdin channel will be used
  if (options.stdinStream) {
    throw new Error('stdinStream not allowed for fork()');
  }

  if (options.customFds) {
    throw new Error('customFds not allowed for fork()');
  }

  // Leave stdin open for the IPC channel. stdout and stderr should be the
  // same as the parent's if silent isn't set.
  options.customFds = (options.silent ? [-1, -1, -1] : [-1, 1, 2]);

  // Just need to set this - child process won't actually use the fd.
  // For backwards compat - this can be changed to 'NODE_CHANNEL' before v0.6.
  options.env = util._extend({}, options.env || process.env);
  options.env.NODE_CHANNEL_FD = 42;

  // stdin is the IPC channel.
  options.stdinStream = createPipe(true);

  var child = spawn(process.execPath, args, options);

  setupChannel(child, options.stdinStream);

  return child;
};


exports._forkChild = function() {
  // set process.send()
  var p = createPipe(true);
  p.open(0);
  setupChannel(process, p);
};


exports.exec = function(command /*, options, callback */) {
  var file, args, options, callback;

  if (typeof arguments[1] === 'function') {
    options = undefined;
    callback = arguments[1];
  } else {
    options = arguments[1];
    callback = arguments[2];
  }

  if (process.platform === 'win32') {
    file = 'cmd.exe';
    args = ['/s', '/c', '"' + command + '"'];
    // Make a shallow copy before patching so we don't clobber the user's
    // options object.
    options = util._extend({}, options);
    options.windowsVerbatimArguments = true;
  } else {
    file = '/bin/sh';
    args = ['-c', command];
  }
  return exports.execFile(file, args, options, callback);
};


exports.execFile = function(file /* args, options, callback */) {
  var args, optionArg, callback;
  var options = {
    encoding: 'utf8',
    timeout: 0,
    maxBuffer: 200 * 1024,
    killSignal: 'SIGTERM',
    cwd: null,
    env: null
  };

  // Parse the parameters.

  if (typeof arguments[arguments.length - 1] === 'function') {
    callback = arguments[arguments.length - 1];
  }

  if (Array.isArray(arguments[1])) {
    args = arguments[1];
    options = util._extend(options, arguments[2]);
  } else {
    args = [];
    options = util._extend(options, arguments[1]);
  }

  var child = spawn(file, args, {
    cwd: options.cwd,
    env: options.env,
    windowsVerbatimArguments: !!options.windowsVerbatimArguments
  });

  var stdout = '';
  var stderr = '';
  var killed = false;
  var exited = false;
  var timeoutId;

  var err;

  function exithandler(code, signal) {
    if (exited) return;
    exited = true;

    if (timeoutId) {
      clearTimeout(timeoutId);
      timeoutId = null;
    }

    if (!callback) return;

    if (err) {
      callback(err, stdout, stderr);
    } else if (code === 0 && signal === null) {
      callback(null, stdout, stderr);
    } else {
      var e = new Error('Command failed: ' + stderr);
      e.killed = child.killed || killed;
      e.code = code;
      e.signal = signal;
      callback(e, stdout, stderr);
    }
  }

  function kill() {
    killed = true;
    child.kill(options.killSignal);
    process.nextTick(function() {
      exithandler(null, options.killSignal);
    });
  }

  if (options.timeout > 0) {
    timeoutId = setTimeout(function() {
      kill();
      timeoutId = null;
    }, options.timeout);
  }

  child.stdout.setEncoding(options.encoding);
  child.stderr.setEncoding(options.encoding);

  child.stdout.addListener('data', function(chunk) {
    stdout += chunk;
    if (stdout.length > options.maxBuffer) {
      err = new Error('maxBuffer exceeded.');
      kill();
    }
  });

  child.stderr.addListener('data', function(chunk) {
    stderr += chunk;
    if (stderr.length > options.maxBuffer) {
      err = new Error('maxBuffer exceeded.');
      kill();
    }
  });

  child.addListener('close', exithandler);

  return child;
};


var spawn = exports.spawn = function(file, args, options) {
  args = args ? args.slice(0) : [];
  args.unshift(file);

  var env = (options ? options.env : null) || process.env;
  var envPairs = [];
  for (var key in env) {
    envPairs.push(key + '=' + env[key]);
  }

  var child = new ChildProcess();

  child.spawn({
    file: file,
    args: args,
    cwd: options ? options.cwd : null,
    windowsVerbatimArguments: !!(options && options.windowsVerbatimArguments),
    envPairs: envPairs,
    customFds: options ? options.customFds : null,
    stdinStream: options ? options.stdinStream : null,
    uid: options ? options.uid : null,
    gid: options ? options.gid : null
  });

  return child;
};


function maybeClose(subprocess) {
  subprocess._closesGot++;

  if (subprocess._closesGot == subprocess._closesNeeded) {
    subprocess.emit('close', subprocess.exitCode, subprocess.signalCode);
  }
}


function ChildProcess() {
  var self = this;

  this._closesNeeded = 1;
  this._closesGot = 0;

  this.signalCode = null;
  this.exitCode = null;
  this.killed = false;

  this._handle = new Process();
  this._handle.owner = this;

  this._handle.onexit = function(exitCode, signalCode) {
    //
    // follow 0.4.x behaviour:
    //
    // - normally terminated processes don't touch this.signalCode
    // - signaled processes don't touch this.exitCode
    //
    if (signalCode) {
      self.signalCode = signalCode;
    } else {
      self.exitCode = exitCode;
    }

    if (self.stdin) {
      self.stdin.destroy();
    }

    self._handle.close();
    self._handle = null;

    self.emit('exit', self.exitCode, self.signalCode);

    maybeClose(self);
  };
}
util.inherits(ChildProcess, EventEmitter);


function setStreamOption(name, index, options) {
  // Skip if we already have options.stdinStream
  if (options[name]) return;

  if (options.customFds &&
      typeof options.customFds[index] == 'number' &&
      options.customFds[index] !== -1) {
    if (options.customFds[index] === index) {
      options[name] = null;
    } else {
      throw new Error('customFds not yet supported');
    }
  } else {
    options[name] = createPipe();
  }
}


ChildProcess.prototype.spawn = function(options) {
  var self = this;

  setStreamOption('stdinStream', 0, options);
  setStreamOption('stdoutStream', 1, options);
  setStreamOption('stderrStream', 2, options);

  var r = this._handle.spawn(options);

  if (r) {
    if (options.stdinStream) {
      options.stdinStream.close();
    }

    if (options.stdoutStream) {
      options.stdoutStream.close();
    }

    if (options.stderrStream) {
      options.stderrStream.close();
    }

    this._handle.close();
    this._handle = null;
    throw errnoException(errno, 'spawn');
  }

  this.pid = this._handle.pid;

  if (options.stdinStream) {
    this.stdin = createSocket(options.stdinStream, false);
  }

  if (options.stdoutStream) {
    this.stdout = createSocket(options.stdoutStream, true);
    this._closesNeeded++;
    this.stdout.on('close', function() {
      maybeClose(self);
    });
  }

  if (options.stderrStream) {
    this.stderr = createSocket(options.stderrStream, true);
    this._closesNeeded++;
    this.stderr.on('close', function() {
      maybeClose(self);
    });
  }

  return r;
};


function errnoException(errorno, syscall, errmsg) {
  // TODO make this more compatible with ErrnoException from src/node.cc
  // Once all of Node is using this function the ErrnoException from
  // src/node.cc should be removed.
  var message = syscall + ' ' + errorno;
  if (errmsg) {
    message += ' - ' + errmsg;
  }
  var e = new Error(message);
  e.errno = e.code = errorno;
  e.syscall = syscall;
  return e;
}


ChildProcess.prototype.kill = function(sig) {
  if (!constants) {
    constants = process.binding('constants');
  }

  sig = sig || 'SIGTERM';
  var signal = constants[sig];

  if (!signal) {
    throw new Error('Unknown signal: ' + sig);
  }

  if (this._handle) {
    this.killed = true;
    var r = this._handle.kill(signal);
    if (r === -1) {
      this.emit('error', errnoException(errno, 'kill'));
      return;
    }
  }
};
