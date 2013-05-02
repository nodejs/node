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
var assert = require('assert');
var dgram = require('dgram');
var fork = require('child_process').fork;
var net = require('net');
var util = require('util');

var cluster = new EventEmitter;
module.exports = cluster;
cluster.Worker = Worker;
cluster.isWorker = ('NODE_UNIQUE_ID' in process.env);
cluster.isMaster = (cluster.isWorker === false);


function Worker() {
  if (!(this instanceof Worker)) return new Worker;
  EventEmitter.call(this);
  this.suicide = undefined;
  this.state = 'none';
  this.id = 0;
}
util.inherits(Worker, EventEmitter);

Worker.prototype.kill = function() {
  this.destroy.apply(this, arguments);
};

Worker.prototype.send = function() {
  this.process.send.apply(this.process, arguments);
};

// Master/worker specific methods are defined in the *Init() functions.


if (cluster.isMaster)
  masterInit();
else
  workerInit();


function createWorkerExecArgv(masterExecArgv, worker) {
  var args = masterExecArgv.slice();
  var debugPort = process.debugPort + worker.id;
  var hasDebugArg = false;

  for (var i = 0; i < args.length; i++) {
    var match = args[i].match(/^(--debug|--debug-brk)(=\d+)?$/);
    if (!match) continue;
    args[i] = match[1] + '=' + debugPort;
    hasDebugArg = true;
  }

  if (!hasDebugArg)
    args = ['--debug-port=' + debugPort].concat(args);

  return args;
}


function masterInit() {
  cluster.workers = {};

  var intercom = new EventEmitter;
  var settings = {
    args: process.argv.slice(2),
    exec: process.argv[1],
    execArgv: process.execArgv,
    silent: false
  };
  cluster.settings = settings;

  // Indexed by address:port:etc key. Its entries are dicts with handle and
  // workers keys. That second one is a list of workers that hold a reference
  // to the handle. When a worker dies, we scan the dicts and close the handle
  // when its reference count drops to zero. Yes, that means we're doing an
  // O(n*m) scan but n and m are small and worker deaths are rare events anyway.
  var handles = {};

  var initialized = false;
  cluster.setupMaster = function(options) {
    if (initialized === true) return;
    initialized = true;
    settings = util._extend(settings, options || {});
    // Tell V8 to write profile data for each process to a separate file.
    // Without --logfile=v8-%p.log, everything ends up in a single, unusable
    // file. (Unusable because what V8 logs are memory addresses and each
    // process has its own memory mappings.)
    if (settings.execArgv.some(function(s) { return /^--prof/.test(s); }) &&
        !settings.execArgv.some(function(s) { return /^--logfile=/.test(s); }))
    {
      settings.execArgv = settings.execArgv.concat(['--logfile=v8-%p.log']);
    }
    cluster.settings = settings;

    process.on('internalMessage', function(message) {
      if (message.cmd !== 'NODE_DEBUG_ENABLED') return;
      for (key in cluster.workers)
        process._debugProcess(cluster.workers[key].process.pid);
    });

    cluster.emit('setup');
  };

  var ids = 0;
  cluster.fork = function(env) {
    cluster.setupMaster();
    var worker = new Worker;
    worker.id = ++ids;
    var workerEnv = util._extend({}, process.env);
    workerEnv = util._extend(workerEnv, env);
    workerEnv.NODE_UNIQUE_ID = '' + worker.id;
    worker.process = fork(settings.exec, settings.args, {
      env: workerEnv,
      silent: settings.silent,
      execArgv: createWorkerExecArgv(settings.execArgv, worker)
    });
    worker.process.once('exit', function(exitCode, signalCode) {
      worker.suicide = !!worker.suicide;
      worker.state = 'dead';
      worker.emit('exit', exitCode, signalCode);
      cluster.emit('exit', worker, exitCode, signalCode);
      delete cluster.workers[worker.id];
    });
    worker.process.once('disconnect', function() {
      worker.suicide = !!worker.suicide;
      worker.state = 'disconnected';
      worker.emit('disconnect');
      cluster.emit('disconnect', worker);
      delete cluster.workers[worker.id];
    });
    worker.process.on('error', worker.emit.bind(worker, 'error'));
    worker.process.on('message', worker.emit.bind(worker, 'message'));
    worker.process.on('internalMessage', internal(worker, onmessage));
    process.nextTick(function() {
      cluster.emit('fork', worker);
    });
    cluster.workers[worker.id] = worker;
    return worker;
  };

  cluster.disconnect = function(cb) {
    for (var key in cluster.workers) {
      var worker = cluster.workers[key];
      worker.disconnect();
    }
    if (cb) intercom.once('disconnect', cb);
  };

  cluster.on('disconnect', function(worker) {
    delete cluster.workers[worker.id];
    // O(n*m) scan but for small values of n and m.
    for (var key in handles) {
      var e = handles[key];
      var i = e.workers.indexOf(worker);
      if (i === -1) continue;
      e.workers.splice(i, 1);
      if (e.workers.length !== 0) continue;
      e.handle.close();
      delete handles[key];
    }
    if (Object.keys(handles).length === 0) {
      intercom.emit('disconnect');
    }
  });

  Worker.prototype.disconnect = function() {
    this.suicide = true;
    send(this, { act: 'disconnect' });
  };

  Worker.prototype.destroy = function(signo) {
    signo = signo || 'SIGTERM';
    var proc = this.process;
    if (proc.connected) {
      proc.once('disconnect', proc.kill.bind(proc, signo));
      proc.disconnect();
      return;
    }
    proc.kill(signo);
  };

  function onmessage(message, handle) {
    var worker = this;
    if (message.act === 'online')
      online(worker);
    else if (message.act === 'queryServer')
      queryServer(worker, message);
    else if (message.act === 'listening')
      listening(worker, message);
    else if (message.act === 'suicide')
      worker.suicide = true;
  }

  function online(worker) {
    worker.state = 'online';
    worker.emit('online');
    cluster.emit('online', worker);
  }

  function queryServer(worker, message) {
    var args = [message.address,
                message.port,
                message.addressType,
                message.fd];
    var key = args.join(':');
    var e = handles[key];
    if (typeof e === 'undefined') {
      e = { workers: [] };
      if (message.addressType === 'udp4' || message.addressType === 'udp6')
        e.handle = dgram._createSocketHandle.apply(null, args);
      else
        e.handle = net._createServerHandle.apply(null, args);
      handles[key] = e;
    }
    e.workers.push(worker);
    send(worker, { ack: message.seq }, e.handle);
  }

  function listening(worker, message) {
    var info = {
      addressType: message.addressType,
      address: message.address,
      port: message.port,
      fd: message.fd
    };
    worker.state = 'listening';
    worker.emit('listening', info);
    cluster.emit('listening', worker, info);
  }

  function send(worker, message, handle, cb) {
    sendHelper(worker.process, message, handle, cb);
  }
}


function workerInit() {
  var handles = [];

  // Called from src/node.js
  cluster._setupWorker = function() {
    var worker = new Worker;
    cluster.worker = worker;
    worker.id = +process.env.NODE_UNIQUE_ID | 0;
    worker.state = 'online';
    worker.process = process;
    process.once('disconnect', process.exit.bind(null, 0));
    process.on('internalMessage', internal(worker, onmessage));
    send({ act: 'online' });
    function onmessage(message, handle) {
      if (message.act === 'disconnect') worker.disconnect();
    }
  };

  // obj is a net#Server or a dgram#Socket object.
  cluster._getServer = function(obj, address, port, addressType, fd, cb) {
    var message = {
      addressType: addressType,
      address: address,
      port: port,
      act: 'queryServer',
      fd: fd
    };
    send(message, function(_, handle) {
      // Monkey-patch the close() method so we can keep track of when it's
      // closed. Avoids resource leaks when the handle is short-lived.
      var close = handle.close;
      handle.close = function() {
        var index = handles.indexOf(handle);
        if (index !== -1) handles.splice(index, 1);
        return close.apply(this, arguments);
      };
      handles.push(handle);
      cb(handle);
    });
    obj.once('listening', function() {
      cluster.worker.state = 'listening';
      message.act = 'listening';
      message.port = obj.address().port || port;
      send(message);
    });
  };

  Worker.prototype.disconnect = function() {
    for (var handle; handle = handles.shift(); handle.close());
    process.disconnect();
  };

  Worker.prototype.destroy = function() {
    if (!process.connected) process.exit(0);
    var exit = process.exit.bind(null, 0);
    send({ act: 'suicide' }, exit);
    process.once('disconnect', exit);
    process.disconnect();
  };

  function send(message, cb) {
    sendHelper(process, message, null, cb);
  }
}


var seq = 0;
var callbacks = {};
function sendHelper(proc, message, handle, cb) {
  // Mark message as internal. See INTERNAL_PREFIX in lib/child_process.js
  message = util._extend({ cmd: 'NODE_CLUSTER' }, message);
  if (cb) callbacks[seq] = cb;
  message.seq = seq;
  seq += 1;
  proc.send(message, handle);
}


// Returns an internalMessage listener that hands off normal messages
// to the callback but intercepts and redirects ACK messages.
function internal(worker, cb) {
  return function(message, handle) {
    if (message.cmd !== 'NODE_CLUSTER') return;
    var fn = cb;
    if (typeof message.ack !== 'undefined') {
      fn = callbacks[message.ack];
      delete callbacks[message.ack];
    }
    fn.apply(worker, arguments);
  };
}
