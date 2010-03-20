var inherits = require('sys').inherits;
var EventEmitter = require('events').EventEmitter;
var Stream = require('net').Stream;
var InternalChildProcess = process.binding('child_process').ChildProcess;


var spawn = exports.spawn = function (path, args, env) {
  var child = new ChildProcess();
  child.spawn(path, args, env);
  return child;
};


exports.exec = function (command, callback) {
  var child = spawn("/bin/sh", ["-c", command]);
  var stdout = "";
  var stderr = "";

  child.stdout.setEncoding('utf8');
  child.stdout.addListener("data", function (chunk) { stdout += chunk; });

  child.stderr.setEncoding('utf8');
  child.stderr.addListener("data", function (chunk) { stderr += chunk; });

  child.addListener("exit", function (code) {
    if (code == 0) {
      if (callback) callback(null, stdout, stderr);
    } else {
      var e = new Error("Command failed: " + stderr);
      e.code = code;
      if (callback) callback(e, stdout, stderr);
    }
  });
};


function ChildProcess () {
  process.EventEmitter.call(this);

  var self = this;

  var gotCHLD = false;
  var exitCode;
  var internal = this._internal = new InternalChildProcess();

  var stdin  = this.stdin  = new Stream();
  var stdout = this.stdout = new Stream();
  var stderr = this.stderr = new Stream();

  stderr.onend = stdout.onend = function () {
    if (gotCHLD && !stdout.readable && !stderr.readable) {
      self.emit('exit', exitCode);
    }
  };

  internal.onexit = function (code) {
    gotCHLD = true;
    exitCode = code;
    if (!stdout.readable && !stderr.readable) {
      self.emit('exit', exitCode);
    }
  };

  this.__defineGetter__('pid', function () { return internal.pid; });
}
inherits(ChildProcess, EventEmitter);


ChildProcess.prototype.kill = function (sig) {
  return this._internal.kill(sig);
};


ChildProcess.prototype.spawn = function (path, args, env) {
  args = args || [];
  env = env || process.env;
  var envPairs = [];
  for (var key in env) {
    if (env.hasOwnProperty(key)) {
      envPairs.push(key + "=" + env[key]);
    }
  }

  var fds = this._internal.spawn(path, args, envPairs);

  this.stdin.open(fds[0]);
  this.stdin.writable = true;
  this.stdin.readable = false;

  this.stdout.open(fds[1]);
  this.stdout.writable = false;
  this.stdout.readable = true;
  this.stdout.resume();

  this.stderr.open(fds[2]);
  this.stderr.writable = false;
  this.stderr.readable = true;
  this.stderr.resume();
};

