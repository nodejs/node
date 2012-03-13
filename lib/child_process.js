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


function setupChannel(target, channel) {
  target._channel = channel;

  var jsonBuffer = '';
  channel.buffering = false;
  channel.onread = function(pool, offset, length, recvHandle) {
    // Update simultaneous accepts on Windows
    net._setSimultaneousAccepts(recvHandle);

    if (pool) {
      jsonBuffer += pool.toString('ascii', offset, offset + length);

      var i, start = 0;

      //Linebreak is used as a message end sign
      while ((i = jsonBuffer.indexOf('\n', start)) >= 0) {
        var json = jsonBuffer.slice(start, i);
        var message = JSON.parse(json);

        //Filter out internal messages
        //if cmd property begin with "_NODE"
        if (message !== null &&
            typeof message === 'object' &&
            typeof message.cmd === 'string' &&
            message.cmd.indexOf('NODE_') === 0) {
          target.emit('internalMessage', message, recvHandle);
        }
        //Non-internal message
        else {
          target.emit('message', message, recvHandle);
        }

        start = i + 1;
      }
      jsonBuffer = jsonBuffer.slice(start);
      this.buffering = jsonBuffer.length !== 0;

    } else {
      this.buffering = false;
      target.disconnect();
    }
  };

  target.send = function(message, sendHandle) {
    if (typeof message === 'undefined') {
      throw new TypeError('message cannot be undefined');
    }

    if (!this.connected) throw new Error('channel closed');

    // For overflow protection don't write if channel queue is too deep.
    if (channel.writeQueueSize > 1024 * 1024) {
      return false;
    }

    var buffer = Buffer(JSON.stringify(message) + '\n');

    // Update simultaneous accepts on Windows
    net._setSimultaneousAccepts(sendHandle);

    var writeReq = channel.write(buffer, 0, buffer.length, sendHandle);

    if (!writeReq) {
      throw errnoException(errno, 'write', 'cannot write to IPC channel.');
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
    options = arguments[2] || {};
  } else {
    args = [];
    options = arguments[1] || {};
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
    if (typeof arguments[2] === 'object') optionArg = arguments[2];
  } else {
    args = [];
    if (typeof arguments[1] === 'object') optionArg = arguments[1];
  }

  // Merge optionArg into options
  util._extend(options, optionArg);

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
    stdinStream: options ? options.stdinStream : null
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

  this._internal = new Process();
  this._internal.onexit = function(exitCode, signalCode) {
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

    self._internal.close();
    self._internal = null;

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

  var r = this._internal.spawn(options);

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

    this._internal.close();
    this._internal = null;
    throw errnoException(errno, 'spawn');
  }

  this.pid = this._internal.pid;

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

  if (this._internal) {
    this.killed = true;
    var r = this._internal.kill(signal);
    // TODO: raise error if r == -1?
  }
};
