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


var spawn = exports.spawn = function(file, args, options) {
  var child = new ChildProcess();

  var args = args ? args.slice(0) : [];
  args.unshift(file);

  child.spawn({
    file: file,
    args: args,
    cwd: options ? options.cwd : null
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


ChildProcess.prototype.kill = function(sig) {
  throw new Error("implement me");
};
