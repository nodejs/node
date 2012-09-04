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

var handleWraps = {};

function handleWrapGetter(name, callback) {
  var cons;

  Object.defineProperty(handleWraps, name, {
    get: function() {
      if (cons !== undefined) return cons;
      return cons = callback();
    }
  });
}

handleWrapGetter('Pipe', function() {
  return process.binding('pipe_wrap').Pipe;
});

handleWrapGetter('TTY', function() {
  return process.binding('tty_wrap').TTY;
});

handleWrapGetter('TCP', function() {
  return process.binding('tcp_wrap').TCP;
});

handleWrapGetter('UDP', function() {
  return process.binding('udp_wrap').UDP;
});

// constructors for lazy loading
function createPipe(ipc) {
  return new handleWraps.Pipe(ipc);
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
  EventEmitter.call(this);

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
  EventEmitter.call(this);

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
      channel.onread = nop;
      channel.close();
      maybeClose(target);
    }
  };

  // object where socket lists will live
  channel.sockets = { got: {}, send: {} };

  // handlers will go through this
  target.on('internalMessage', function(message, handle) {
    if (message.cmd !== 'NODE_HANDLE') return;

    var obj = handleConversion[message.type];

    // Update simultaneous accepts on Windows
    if (process.platform === 'win32') {
      handle._simultaneousAccepts = false;
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

    /* If the master is > 2 read() calls behind, please stop sending. */
    return channel.writeQueueSize < (65536 * 2);
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

  // Leave stdin open for the IPC channel. stdout and stderr should be the
  // same as the parent's if silent isn't set.
  options.stdio = options.silent ? ['pipe', 'pipe', 'pipe', 'ipc'] :
      [0, 1, 2, 'ipc'];

  return spawn(process.execPath, args, options);
};


exports._forkChild = function(fd) {
  // set process.send()
  var p = createPipe(true);
  p.open(fd);
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

  function errorhandler(e) {
    err = e;
    child.stdout.destroy();
    child.stderr.destroy();
    exithandler();
  }

  function kill() {
    child.stdout.destroy();
    child.stderr.destroy();

    killed = true;
    try {
      child.kill(options.killSignal);
    } catch (e) {
      err = e;
      exithandler();
    }
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
      err = new Error('stdout maxBuffer exceeded.');
      kill();
    }
  });

  child.stderr.addListener('data', function(chunk) {
    stderr += chunk;
    if (stderr.length > options.maxBuffer) {
      err = new Error('stderr maxBuffer exceeded.');
      kill();
    }
  });

  child.addListener('close', exithandler);
  child.addListener('error', errorhandler);

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
  if (options && options.customFds && !options.stdio) {
    options.stdio = options.customFds.map(function(fd) {
      return fd === -1 ? 'pipe' : fd;
    });
  }

  child.spawn({
    file: file,
    args: args,
    cwd: options ? options.cwd : null,
    windowsVerbatimArguments: !!(options && options.windowsVerbatimArguments),
    detached: !!(options && options.detached),
    envPairs: envPairs,
    stdio: options ? options.stdio : null,
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
  EventEmitter.call(this);

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
    // new in 0.9.x:
    //
    // - spawn failures are reported with exitCode == -1
    //
    var err = (exitCode == -1) ? errnoException(errno, 'spawn') : null;

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

    if (exitCode == -1) {
      self.emit('error', err);
    } else {
      self.emit('exit', self.exitCode, self.signalCode);
    }

    maybeClose(self);
  };
}
util.inherits(ChildProcess, EventEmitter);


function getHandleWrapType(stream) {
  if (stream instanceof handleWraps.Pipe) return 'pipe';
  if (stream instanceof handleWraps.TTY) return 'tty';
  if (stream instanceof handleWraps.TCP) return 'tcp';
  if (stream instanceof handleWraps.UDP) return 'udp';

  return false;
}


ChildProcess.prototype.spawn = function(options) {
  var self = this,
      ipc,
      ipcFd,
      // If no `stdio` option was given - use default
      stdio = options.stdio || 'pipe';

  // Replace shortcut with an array
  if (typeof stdio === 'string') {
    switch (stdio) {
      case 'ignore': stdio = ['ignore', 'ignore', 'ignore']; break;
      case 'pipe': stdio = ['pipe', 'pipe', 'pipe']; break;
      case 'inherit': stdio = [0, 1, 2]; break;
      default: throw new TypeError('Incorrect value of stdio option: ' + stdio);
    }
  } else if (!Array.isArray(stdio)) {
    throw new TypeError('Incorrect value of stdio option: ' + stdio);
  }

  // At least 3 stdio will be created
  // Don't concat() a new Array() because it would be sparse, and
  // stdio.reduce() would skip the sparse elements of stdio.
  // See http://stackoverflow.com/a/5501711/3561
  while (stdio.length < 3) stdio.push(undefined);

  // Translate stdio into C++-readable form
  // (i.e. PipeWraps or fds)
  stdio = stdio.reduce(function(acc, stdio, i) {
    function cleanup() {
      acc.filter(function(stdio) {
        return stdio.type === 'pipe' || stdio.type === 'ipc';
      }).forEach(function(stdio) {
        stdio.handle.close();
      });
    }

    // Defaults
    if (stdio === undefined || stdio === null) {
      stdio = i < 3 ? 'pipe' : 'ignore';
    }

    if (stdio === 'ignore') {
      acc.push({type: 'ignore'});
    } else if (stdio === 'pipe' || typeof stdio === 'number' && stdio < 0) {
      acc.push({type: 'pipe', handle: createPipe()});
    } else if (stdio === 'ipc') {
      if (ipc !== undefined) {
        // Cleanup previously created pipes
        cleanup();
        throw Error('Child process can have only one IPC pipe');
      }

      ipc = createPipe(true);
      ipcFd = i;

      acc.push({ type: 'pipe', handle: ipc, ipc: true });
    } else if (typeof stdio === 'number' || typeof stdio.fd === 'number') {
      acc.push({ type: 'fd', fd: stdio.fd || stdio });
    } else if (getHandleWrapType(stdio) || getHandleWrapType(stdio.handle) ||
               getHandleWrapType(stdio._handle)) {
      var handle = getHandleWrapType(stdio) ?
          stdio :
          getHandleWrapType(stdio.handle) ? stdio.handle : stdio._handle;

      acc.push({
        type: 'wrap',
        wrapType: getHandleWrapType(handle),
        handle: handle
      });
    } else {
      // Cleanup
      cleanup();
      throw new TypeError('Incorrect value for stdio stream: ' + stdio);
    }

    return acc;
  }, []);

  options.stdio = stdio;

  if (ipc !== undefined) {
    // Let child process know about opened IPC channel
    options.envPairs = options.envPairs || [];
    options.envPairs.push('NODE_CHANNEL_FD=' + ipcFd);
  }

  var r = this._handle.spawn(options);

  if (r) {
    // Close all opened fds on error
    stdio.forEach(function(stdio) {
      if (stdio.type === 'pipe') {
        stdio.handle.close();
      }
    });

    this._handle.close();
    this._handle = null;
    throw errnoException(errno, 'spawn');
  }

  this.pid = this._handle.pid;

  stdio.forEach(function(stdio, i) {
    if (stdio.type === 'ignore') return;

    if (stdio.ipc) {
      self._closesNeeded++;
      return;
    }

    if (stdio.handle) {
      // when i === 0 - we're dealing with stdin
      // (which is the only one writable pipe)
      stdio.socket = createSocket(stdio.handle, i > 0);

      if (i > 0) {
        self._closesNeeded++;
        stdio.socket.on('close', function() {
          maybeClose(self);
        });
      }
    }
  });

  this.stdin = stdio.length >= 1 && stdio[0].socket !== undefined ?
      stdio[0].socket : null;
  this.stdout = stdio.length >= 2 && stdio[1].socket !== undefined ?
      stdio[1].socket : null;
  this.stderr = stdio.length >= 3 && stdio[2].socket !== undefined ?
      stdio[2].socket : null;

  this.stdio = stdio.map(function(stdio) {
    return stdio.socket === undefined ? null : stdio.socket;
  });

  // Add .send() method and start listening for IPC data
  if (ipc !== undefined) setupChannel(this, ipc);

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
  var signal;

  if (!constants) {
    constants = process.binding('constants');
  }

  if (sig === 0) {
    signal = 0;
  } else if (!sig) {
    signal = constants['SIGTERM'];
  } else {
    signal = constants[sig];
  }

  if (signal === undefined) {
    throw new Error('Unknown signal: ' + sig);
  }

  if (this._handle) {
    var r = this._handle.kill(signal);
    if (r == 0) {
      /* Success. */
      this.killed = true;
      return true;
    } else if (errno == 'ESRCH') {
      /* Already dead. */
    } else if (errno == 'EINVAL' || errno == 'ENOSYS') {
      /* The underlying platform doesn't support this signal. */
      throw errnoException(errno, 'kill');
    } else {
      /* Other error, almost certainly EPERM. */
      this.emit('error', errnoException(errno, 'kill'));
    }
  }

  /* Kill didn't succeed. */
  return false;
};


ChildProcess.prototype.ref = function() {
  if (this._handle) this._handle.ref();
};


ChildProcess.prototype.unref = function() {
  if (this._handle) this._handle.unref();
};
