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

var StringDecoder = require('string_decoder').StringDecoder;
var EventEmitter = require('events').EventEmitter;
var net = require('net');
var dgram = require('dgram');
var assert = require('assert');
var util = require('util');

var Process = process.binding('process_wrap').Process;
var uv = process.binding('uv');

var constants;  // Lazy-loaded process.binding('constants')

var errnoException = util._errnoException;
var handleWraps = {};

function handleWrapGetter(name, callback) {
  var cons;

  Object.defineProperty(handleWraps, name, {
    get: function() {
      if (!util.isUndefined(cons)) return cons;
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
      var server = new net.Server();
      server.listen(handle, function() {
        emit(server);
      });
    }
  },

  'net.Socket': {
    send: function(message, socket) {
      // if the socket was created by net.Server
      if (socket.server) {
        // the slave should keep track of the socket
        message.key = socket.server._connectionKey;

        var firstTime = !this._channel.sockets.send[message.key];
        var socketList = getSocketList('send', this, message.key);

        // the server should no longer expose a .connection property
        // and when asked to close it should query the socket status from
        // the slaves
        if (firstTime) socket.server._setupSlave(socketList);

        // Act like socket is detached
        socket.server._connections--;
      }

      // remove handle from socket object, it will be closed when the socket
      // will be sent
      var handle = socket._handle;
      handle.onread = function() {};
      socket._handle = null;

      return handle;
    },

    postSend: function(handle) {
      // Close the Socket handle after sending it
      handle.close();
    },

    got: function(message, handle, emit) {
      var socket = new net.Socket({handle: handle});
      socket.readable = socket.writable = true;

      // if the socket was created by net.Server we will track the socket
      if (message.key) {

        // add socket to connections list
        var socketList = getSocketList('got', this, message.key);
        socketList.add({
          socket: socket
        });
      }

      emit(socket);
    }
  },

  'dgram.Native': {
    simultaneousAccepts: false,

    send: function(message, handle) {
      return handle;
    },

    got: function(message, handle, emit) {
      emit(handle);
    }
  },

  'dgram.Socket': {
    simultaneousAccepts: false,

    send: function(message, socket) {
      message.dgramType = socket.type;

      return socket._handle;
    },

    got: function(message, handle, emit) {
      var socket = new dgram.Socket(message.dgramType);

      socket.bind(handle, function() {
        emit(socket);
      });
    }
  }
};

// This object keep track of the socket there are sended
function SocketListSend(slave, key) {
  EventEmitter.call(this);

  this.key = key;
  this.slave = slave;
}
util.inherits(SocketListSend, EventEmitter);

SocketListSend.prototype._request = function(msg, cmd, callback) {
  var self = this;

  if (!this.slave.connected) return onclose();
  this.slave.send(msg);

  function onclose() {
    self.slave.removeListener('internalMessage', onreply);
    callback(new Error('Slave closed before reply'));
  };

  function onreply(msg) {
    if (!(msg.cmd === cmd && msg.key === self.key)) return;
    self.slave.removeListener('disconnect', onclose);
    self.slave.removeListener('internalMessage', onreply);

    callback(null, msg);
  };

  this.slave.once('disconnect', onclose);
  this.slave.on('internalMessage', onreply);
};

SocketListSend.prototype.close = function close(callback) {
  this._request({
    cmd: 'NODE_SOCKET_NOTIFY_CLOSE',
    key: this.key
  }, 'NODE_SOCKET_ALL_CLOSED', callback);
};

SocketListSend.prototype.getConnections = function getConnections(callback) {
  this._request({
    cmd: 'NODE_SOCKET_GET_COUNT',
    key: this.key
  }, 'NODE_SOCKET_COUNT', function(err, msg) {
    if (err) return callback(err);
    callback(null, msg.count);
  });
};

// This object keep track of the socket there are received
function SocketListReceive(slave, key) {
  EventEmitter.call(this);

  var self = this;

  this.connections = 0;
  this.key = key;
  this.slave = slave;

  function onempty() {
    if (!self.slave.connected) return;

    self.slave.send({
      cmd: 'NODE_SOCKET_ALL_CLOSED',
      key: self.key
    });
  }

  this.slave.on('internalMessage', function(msg) {
    if (msg.key !== self.key) return;

    if (msg.cmd === 'NODE_SOCKET_NOTIFY_CLOSE') {
      // Already empty
      if (self.connections === 0) return onempty();

      // Wait for sockets to get closed
      self.once('empty', onempty);
    } else if (msg.cmd === 'NODE_SOCKET_GET_COUNT') {
      if (!self.slave.connected) return;
      self.slave.send({
        cmd: 'NODE_SOCKET_COUNT',
        key: self.key,
        count: self.connections
      });
    }
  });
}
util.inherits(SocketListReceive, EventEmitter);

SocketListReceive.prototype.add = function(obj) {
  var self = this;

  this.connections++;

  // Notify previous owner of socket about its state change
  obj.socket.once('close', function() {
    self.connections--;

    if (self.connections === 0) self.emit('empty');
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

var INTERNAL_PREFIX = 'NODE_';
function handleMessage(target, message, handle) {
  var eventName = 'message';
  if (!util.isNull(message) &&
      util.isObject(message) &&
      util.isString(message.cmd) &&
      message.cmd.length > INTERNAL_PREFIX.length &&
      message.cmd.slice(0, INTERNAL_PREFIX.length) === INTERNAL_PREFIX) {
    eventName = 'internalMessage';
  }
  target.emit(eventName, message, handle);
}

function setupChannel(target, channel) {
  target._channel = channel;
  target._handleQueue = null;

  var decoder = new StringDecoder('utf8');
  var jsonBuffer = '';
  channel.buffering = false;
  channel.onread = function(nread, pool, recvHandle) {
    // TODO(bnoordhuis) Check that nread > 0.
    if (pool) {
      jsonBuffer += decoder.write(pool);

      var i, start = 0;

      //Linebreak is used as a message end sign
      while ((i = jsonBuffer.indexOf('\n', start)) >= 0) {
        var json = jsonBuffer.slice(start, i);
        var message = JSON.parse(json);

        // There will be at most one NODE_HANDLE message in every chunk we
        // read because SCM_RIGHTS messages don't get coalesced. Make sure
        // that we deliver the handle with the right message however.
        if (message && message.cmd === 'NODE_HANDLE')
          handleMessage(target, message, recvHandle);
        else
          handleMessage(target, message, undefined);

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
    // Once acknowledged - continue sending handles.
    if (message.cmd === 'NODE_HANDLE_ACK') {
      assert(util.isArray(target._handleQueue));
      var queue = target._handleQueue;
      target._handleQueue = null;

      queue.forEach(function(args) {
        target._send(args.message, args.handle, false);
      });

      // Process a pending disconnect (if any).
      if (!target.connected && target._channel && !target._handleQueue)
        target._disconnect();

      return;
    }

    if (message.cmd !== 'NODE_HANDLE') return;

    // Acknowledge handle receival. Don't emit error events (for example if
    // the other side has disconnected) because this call to send() is not
    // initiated by the user and it shouldn't be fatal to be unable to ACK
    // a message.
    target._send({ cmd: 'NODE_HANDLE_ACK' }, null, true);

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
    if (!this.connected)
      this.emit('error', new Error('channel closed'));
    else
      this._send(message, handle, false);
  };

  target._send = function(message, handle, swallowErrors) {
    assert(this.connected || this._channel);

    if (util.isUndefined(message))
      throw new TypeError('message cannot be undefined');

    // package messages with a handle object
    if (handle) {
      // this message will be handled by an internalMessage event handler
      message = {
        cmd: 'NODE_HANDLE',
        type: null,
        msg: message
      };

      if (handle instanceof net.Socket) {
        message.type = 'net.Socket';
      } else if (handle instanceof net.Server) {
        message.type = 'net.Server';
      } else if (handle instanceof process.binding('tcp_wrap').TCP ||
                 handle instanceof process.binding('pipe_wrap').Pipe) {
        message.type = 'net.Native';
      } else if (handle instanceof dgram.Socket) {
        message.type = 'dgram.Socket';
      } else if (handle instanceof process.binding('udp_wrap').UDP) {
        message.type = 'dgram.Native';
      } else {
        throw new TypeError("This handle type can't be sent");
      }

      // Queue-up message and handle if we haven't received ACK yet.
      if (this._handleQueue) {
        this._handleQueue.push({ message: message.msg, handle: handle });
        return;
      }

      var obj = handleConversion[message.type];

      // convert TCP object to native handle object
      handle = handleConversion[message.type].send.apply(target, arguments);

      // Update simultaneous accepts on Windows
      if (obj.simultaneousAccepts) {
        net._setSimultaneousAccepts(handle);
      }
    } else if (this._handleQueue) {
      // Queue request anyway to avoid out-of-order messages.
      this._handleQueue.push({ message: message, handle: null });
      return;
    }

    var req = { oncomplete: nop };
    var string = JSON.stringify(message) + '\n';
    var err = channel.writeUtf8String(req, string, handle);

    if (err) {
      if (!swallowErrors)
        this.emit('error', errnoException(err, 'write'));
    } else if (handle && !this._handleQueue) {
      this._handleQueue = [];
    }

    if (obj && obj.postSend) {
      req.oncomplete = obj.postSend.bind(null, handle);
    }

    /* If the master is > 2 read() calls behind, please stop sending. */
    return channel.writeQueueSize < (65536 * 2);
  };

  // connected will be set to false immediately when a disconnect() is
  // requested, even though the channel might still be alive internally to
  // process queued messages. The three states are distinguished as follows:
  // - disconnect() never requested: _channel is not null and connected
  //   is true
  // - disconnect() requested, messages in the queue: _channel is not null
  //   and connected is false
  // - disconnect() requested, channel actually disconnected: _channel is
  //   null and connected is false
  target.connected = true;

  target.disconnect = function() {
    if (!this.connected) {
      this.emit('error', new Error('IPC channel is already disconnected'));
      return;
    }

    // Do not allow any new messages to be written.
    this.connected = false;

    // If there are no queued messages, disconnect immediately. Otherwise,
    // postpone the disconnect so that it happens internally after the
    // queue is flushed.
    if (!this._handleQueue)
      this._disconnect();
  }

  target._disconnect = function() {
    assert(this._channel);

    // This marks the fact that the channel is actually disconnected.
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

    process.nextTick(finish);
  };

  channel.readStart();
}


function nop() { }

exports.fork = function(modulePath /*, args, options*/) {

  // Get options and args arguments.
  var options, args, execArgv;
  if (util.isArray(arguments[1])) {
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

  options.execPath = options.execPath || process.execPath;

  return spawn(options.execPath, args, options);
};


exports._forkChild = function(fd) {
  // set process.send()
  var p = createPipe(true);
  p.open(fd);
  p.unref();
  setupChannel(process, p);

  var refs = 0;
  process.on('newListener', function(name) {
    if (name !== 'message' && name !== 'disconnect') return;
    if (++refs === 1) p.ref();
  });
  process.on('removeListener', function(name) {
    if (name !== 'message' && name !== 'disconnect') return;
    if (--refs === 0) p.unref();
  });
};


exports.exec = function(command /*, options, callback */) {
  var file, args, options, callback;

  if (util.isFunction(arguments[1])) {
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

  if (options && options.shell)
    file = options.shell;

  return exports.execFile(file, args, options, callback);
};


exports.execFile = function(file /* args, options, callback */) {
  var args, callback;
  var options = {
    encoding: 'utf8',
    timeout: 0,
    maxBuffer: 200 * 1024,
    killSignal: 'SIGTERM',
    cwd: null,
    env: null
  };

  // Parse the parameters.

  if (util.isFunction(arguments[arguments.length - 1])) {
    callback = arguments[arguments.length - 1];
  }

  if (util.isArray(arguments[1])) {
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

  var encoding;
  var _stdout;
  var _stderr;
  if (options.encoding !== 'buffer' && Buffer.isEncoding(options.encoding)) {
    encoding = options.encoding;
    _stdout = '';
    _stderr = '';
  } else {
    _stdout = [];
    _stderr = [];
    encoding = null;
  }
  var stdoutLen = 0;
  var stderrLen = 0;
  var killed = false;
  var exited = false;
  var timeoutId;

  var ex = null;

  function exithandler(code, signal) {
    if (exited) return;
    exited = true;

    if (timeoutId) {
      clearTimeout(timeoutId);
      timeoutId = null;
    }

    if (!callback) return;

    // merge chunks
    var stdout;
    var stderr;
    if (!encoding) {
      stdout = Buffer.concat(_stdout);
      stderr = Buffer.concat(_stderr);
    } else {
      stdout = _stdout;
      stderr = _stderr;
    }

    if (ex) {
      // Will be handled later
    } else if (code === 0 && signal === null) {
      callback(null, stdout, stderr);
      return;
    }

    var cmd = file;
    if (args.length !== 0)
      cmd += ' ' + args.join(' ');

    if (!ex) {
      ex = new Error('Command failed: ' + cmd + '\n' + stderr);
      ex.killed = child.killed || killed;
      ex.code = code < 0 ? uv.errname(code) : code;
      ex.signal = signal;
    }

    ex.cmd = cmd;
    callback(ex, stdout, stderr);
  }

  function errorhandler(e) {
    ex = e;
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
      ex = e;
      exithandler();
    }
  }

  if (options.timeout > 0) {
    timeoutId = setTimeout(function() {
      kill();
      timeoutId = null;
    }, options.timeout);
  }

  child.stdout.addListener('data', function(chunk) {
    stdoutLen += chunk.length;

    if (stdoutLen > options.maxBuffer) {
      ex = new Error('stdout maxBuffer exceeded.');
      kill();
    } else {
      if (!encoding)
        _stdout.push(chunk);
      else
        _stdout += chunk;
    }
  });

  child.stderr.addListener('data', function(chunk) {
    stderrLen += chunk.length;

    if (stderrLen > options.maxBuffer) {
      ex = new Error('stderr maxBuffer exceeded.');
      kill();
    } else {
      if (!encoding)
        _stderr.push(chunk);
      else
        _stderr += chunk;
    }
  });

  if (encoding) {
    child.stderr.setEncoding(encoding);
    child.stdout.setEncoding(encoding);
  }

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

  // Initialize TCPWrap and PipeWrap
  process.binding('tcp_wrap');
  process.binding('pipe_wrap');

  var self = this;

  this._closesNeeded = 1;
  this._closesGot = 0;
  this.connected = false;

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
    // - spawn failures are reported with exitCode < 0
    //
    var err = (exitCode < 0) ? errnoException(exitCode, 'spawn') : null;

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

    if (exitCode < 0) {
      self.emit('error', err);
    } else {
      self.emit('exit', self.exitCode, self.signalCode);
    }

    // if any of the stdio streams have not been touched,
    // then pull all the data through so that it can get the
    // eof and emit a 'close' event.
    // Do it on nextTick so that the user has one last chance
    // to consume the output, if for example they only want to
    // start reading the data once the process exits.
    process.nextTick(function() {
      flushStdio(self);
    });

    maybeClose(self);
  };
}
util.inherits(ChildProcess, EventEmitter);


function flushStdio(subprocess) {
  subprocess.stdio.forEach(function(stream, fd, stdio) {
    if (!stream || !stream.readable || stream._consuming ||
        stream._readableState.flowing)
      return;
    stream.resume();
  });
}



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
  if (util.isString(stdio)) {
    switch (stdio) {
      case 'ignore': stdio = ['ignore', 'ignore', 'ignore']; break;
      case 'pipe': stdio = ['pipe', 'pipe', 'pipe']; break;
      case 'inherit': stdio = [0, 1, 2]; break;
      default: throw new TypeError('Incorrect value of stdio option: ' + stdio);
    }
  } else if (!util.isArray(stdio)) {
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
    if (util.isNullOrUndefined(stdio)) {
      stdio = i < 3 ? 'pipe' : 'ignore';
    }

    if (stdio === 'ignore') {
      acc.push({type: 'ignore'});
    } else if (stdio === 'pipe' || util.isNumber(stdio) && stdio < 0) {
      acc.push({type: 'pipe', handle: createPipe()});
    } else if (stdio === 'ipc') {
      if (!util.isUndefined(ipc)) {
        // Cleanup previously created pipes
        cleanup();
        throw Error('Child process can have only one IPC pipe');
      }

      ipc = createPipe(true);
      ipcFd = i;

      acc.push({ type: 'pipe', handle: ipc, ipc: true });
    } else if (util.isNumber(stdio) || util.isNumber(stdio.fd)) {
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

  if (!util.isUndefined(ipc)) {
    // Let child process know about opened IPC channel
    options.envPairs = options.envPairs || [];
    options.envPairs.push('NODE_CHANNEL_FD=' + ipcFd);
  }

  var err = this._handle.spawn(options);

  if (err == uv.UV_ENOENT) {
    process.nextTick(function() {
      self._handle.onexit(err);
    });
  } else if (err) {
    // Close all opened fds on error
    stdio.forEach(function(stdio) {
      if (stdio.type === 'pipe') {
        stdio.handle.close();
      }
    });

    this._handle.close();
    this._handle = null;
    throw errnoException(err, 'spawn');
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
      stdio.socket = createSocket(self.pid !== 0 ? stdio.handle : null, i > 0);

      if (i > 0 && self.pid !== 0) {
        self._closesNeeded++;
        stdio.socket.on('close', function() {
          maybeClose(self);
        });
      }
    }
  });

  this.stdin = stdio.length >= 1 && !util.isUndefined(stdio[0].socket) ?
      stdio[0].socket : null;
  this.stdout = stdio.length >= 2 && !util.isUndefined(stdio[1].socket) ?
      stdio[1].socket : null;
  this.stderr = stdio.length >= 3 && !util.isUndefined(stdio[2].socket) ?
      stdio[2].socket : null;

  this.stdio = stdio.map(function(stdio) {
    return util.isUndefined(stdio.socket) ? null : stdio.socket;
  });

  // Add .send() method and start listening for IPC data
  if (!util.isUndefined(ipc)) setupChannel(this, ipc);

  return err;
};


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

  if (util.isUndefined(signal)) {
    throw new Error('Unknown signal: ' + sig);
  }

  if (this._handle) {
    var err = this._handle.kill(signal);
    if (err === 0) {
      /* Success. */
      this.killed = true;
      return true;
    }
    if (err === uv.UV_ESRCH) {
      /* Already dead. */
    } else if (err === uv.UV_EINVAL || err === uv.UV_ENOSYS) {
      /* The underlying platform doesn't support this signal. */
      throw errnoException(err, 'kill');
    } else {
      /* Other error, almost certainly EPERM. */
      this.emit('error', errnoException(err, 'kill'));
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
