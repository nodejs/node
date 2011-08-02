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
var Process = process.binding('process_wrap').Process;
var inherits = require('util').inherits;


// constructors for lazy loading
function createPipe() {
  var Pipe = process.binding('pipe_wrap').Pipe;
  return new Pipe();
}

function createSocket(pipe, readable) {
  var Socket = require('net').Socket;
  var s = new Socket({ handle: pipe });

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


exports.exec = function(command /*, options, callback */) {
  var rest = Array.prototype.slice.call(arguments, 1);
  var args;
  if (process.platform == 'win32') {
    args = ['cmd.exe', ['/c', command]].concat(rest);
  } else {
    args = ['/bin/sh', ['-c', command]].concat(rest);
  }
  return exports.execFile.apply(this, args);
};


exports.execFile = function(file /* args, options, callback */) {
  var args, optionArg, callback;
  var options = {
    encoding: 'utf8',
    timeout: 0,
    maxBuffer: 200 * 1024,
    killSignal: 'SIGTERM',
    setsid: false,
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
  if (optionArg) {
    var keys = Object.keys(options);
    for (var i = 0, len = keys.length; i < len; i++) {
      var k = keys[i];
      if (optionArg[k] !== undefined) options[k] = optionArg[k];
    }
  }

  var child = spawn(file, args, {
    cwd: options.cwd,
    env: options.env
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

  child.addListener('exit', exithandler);

  return child;
};


var spawn = exports.spawn = function(file, args, options) {
  var child = new ChildProcess();

  var args = args ? args.slice(0) : [];
  args.unshift(file);

  var env = (options ? options.env : null) || process.env;
  var envPairs = [];
  var keys = Object.keys(env);
  for (var key in env) {
    envPairs.push(key + '=' + env[key]);
  }

  child.spawn({
    file: file,
    args: args,
    cwd: options ? options.cwd : null,
    envPairs: envPairs
  });

  return child;
};


function maybeExit(subprocess) {
  subprocess._closesGot++;

  if (subprocess._closesGot == subprocess._closesNeeded) {
    subprocess.emit('exit', subprocess.exitCode, subprocess.signalCode);
  }
}


function ChildProcess() {
  var self = this;

  this._closesNeeded = 1;
  this._closesGot = 0;

  this.signalCode = null;
  this.exitCode = null;

  this._internal = new Process();
  this._internal.onexit = function(exitCode, signalCode) {
    if (signalCode) self.signalCode = signalCode;
    self.exitCode = exitCode;

    if (self.stdin) {
      self.stdin.destroy();
    }

    self._internal.close();
    self._internal = null;

    maybeExit(self);
  };
}
inherits(ChildProcess, EventEmitter);


function setStreamOption(name, index, options) {
  if (options.customFds &&
      typeof options.customFds[index] == 'number' &&
      options.customFds[index] !== -1) {
    if (options.customFds[index] === index) {
      options[name] = null;
    } else {
      throw new Error("customFds not yet supported");
    }
  } else {
    options[name] = createPipe();
  }
}


ChildProcess.prototype.spawn = function(options) {
  var self = this;

  setStreamOption("stdinStream", 0, options);
  setStreamOption("stdoutStream", 1, options);
  setStreamOption("stderrStream", 2, options);

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
    throw errnoException("spawn", errno)
  }

  this.pid = this._internal.pid;

  if (options.stdinStream) {
    this.stdin = createSocket(options.stdinStream, false);
  }

  if (options.stdoutStream) {
    this.stdout = createSocket(options.stdoutStream, true);
    this._closesNeeded++;
    this.stdout.on('close', function() {
      maybeExit(self);
    });
  }

  if (options.stderrStream) {
    this.stderr = createSocket(options.stderrStream, true);
    this._closesNeeded++;
    this.stderr.on('close', function() {
      maybeExit(self);
    });
  }

  return r;
};

function errnoException(errorno, syscall) {
  // TODO make this more compatible with ErrnoException from src/node.cc
  // Once all of Node is using this function the ErrnoException from
  // src/node.cc should be removed.
  var e = new Error(syscall + ' ' + errorno);
  e.errno = e.code = errorno;
  e.syscall = syscall;
  return e;
}


ChildProcess.prototype.kill = function(sig) {
  throw new Error("implement me");
};
