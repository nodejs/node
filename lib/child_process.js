var inherits = require('sys').inherits;
var EventEmitter = require('events').EventEmitter;
var Stream = require('net').Stream;
var InternalChildProcess = process.binding('child_process').ChildProcess;


var spawn = exports.spawn = function (path, args, env, customFds) {
  var child = new ChildProcess();
  child.spawn(path, args, env, customFds);
  return child;
};

exports.exec = function (command /*, options, callback */) {
  if (arguments.length < 3) {
    return exports.execFile("/bin/sh", ["-c", command], arguments[1]);
  } else {
    return exports.execFile("/bin/sh", ["-c", command], arguments[1], arguments[2]);
  }
};

exports.execFile = function (file, args /*, options, callback */) {
  var options = { encoding: 'utf8'
                , timeout: 0
                , maxBuffer: 200*1024
                , killSignal: 'SIGKILL'
                };

  var callback = arguments[arguments.length-1];

  if (typeof arguments[2] == 'object') {
    var keys = Object.keys(options);
    for (var i = 0; i < keys.length; i++) {
      var k = keys[i];
      if (arguments[2][k] !== undefined) options[k] = arguments[2][k];
    }
  }

  var child = spawn(file, args);
  var stdout = "";
  var stderr = "";
  var killed = false;

  var timeoutId;
  if (options.timeout > 0) {
    timeoutId = setTimeout(function () {
      if (!killed) {
        child.kill(options.killSignal);
        killed = true;
        timeoutId = null;
      }
    }, options.timeout);
  }

  child.stdout.setEncoding(options.encoding);
  child.stderr.setEncoding(options.encoding);

  child.stdout.addListener("data", function (chunk) {
    stdout += chunk;
    if (!killed && stdout.length > options.maxBuffer) {
      child.kill(options.killSignal);
      killed = true;
    }
  });

  child.stderr.addListener("data", function (chunk) {
    stderr += chunk;
    if (!killed && stderr.length > options.maxBuffer) {
      child.kill(options.killSignal);
      killed = true
    }
  });

  child.addListener("exit", function (code, signal) {
    if (timeoutId) clearTimeout(timeoutId);
    if (code === 0 && signal === null) {
      if (callback) callback(null, stdout, stderr);
    } else {
      var e = new Error("Command failed: " + stderr);
      e.killed = killed;
      e.code = code;
      e.signal = signal;
      if (callback) callback(e, stdout, stderr);
    }
  });
};


function ChildProcess () {
  EventEmitter.call(this);

  var self = this;

  var gotCHLD = false;
  var exitCode;
  var termSignal;
  var internal = this._internal = new InternalChildProcess();

  var stdin  = this.stdin  = new Stream();
  var stdout = this.stdout = new Stream();
  var stderr = this.stderr = new Stream();

  var stderrClosed = false;
  var stdoutClosed = false;

  stderr.addListener('close', function () {
    stderrClosed = true;
    if (gotCHLD && (!self.stdout || stdoutClosed)) {
      self.emit('exit', exitCode, termSignal);
    }
  });

  stdout.addListener('close', function () {
    stdoutClosed = true;
    if (gotCHLD && (!self.stderr || stderrClosed)) {
      self.emit('exit', exitCode, termSignal);
    }
  });

  internal.onexit = function (code, signal) {
    gotCHLD = true;
    exitCode = code;
    termSignal = signal;
    if (self.stdin) {
      self.stdin.end();
    }
    if (   (!self.stdout || !self.stdout.readable)
        && (!self.stderr || !self.stderr.readable)) {
      self.emit('exit', exitCode, termSignal);
    }
  };

  this.__defineGetter__('pid', function () { return internal.pid; });
}
inherits(ChildProcess, EventEmitter);


ChildProcess.prototype.kill = function (sig) {
  return this._internal.kill(sig);
};


ChildProcess.prototype.spawn = function (path, args, env, customFds) {
  args = args || [];
  env = env || process.env;
  var envPairs = [];
  var keys = Object.keys(env);
  for (var index = 0, keysLength = keys.length; index < keysLength; index++) {
    var key = keys[index];
    envPairs.push(key + "=" + env[key]);
  }

  customFds = customFds || [-1, -1, -1];
  var fds = this.fds = this._internal.spawn(path, args, envPairs, customFds);

  if (customFds[0] === -1 || customFds[0] === undefined) {
    this.stdin.open(fds[0]);
    this.stdin.writable = true;
    this.stdin.readable = false;
  }
  else {
    this.stdin = null;
  }

  if (customFds[1] === -1 || customFds[1] === undefined) {
    this.stdout.open(fds[1]);
    this.stdout.writable = false;
    this.stdout.readable = true;
    this.stdout.resume();
  }
  else {
    this.stdout = null;
  }

  if (customFds[2] === -1 || customFds[2] === undefined) {
    this.stderr.open(fds[2]);
    this.stderr.writable = false;
    this.stderr.readable = true;
    this.stderr.resume();
  }
  else {
    this.stderr = null;
  }
};

