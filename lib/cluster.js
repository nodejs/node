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
var SCHED_NONE = 1;
var SCHED_RR = 2;

var cluster = new EventEmitter;
module.exports = cluster;
cluster.Worker = Worker;
cluster.isWorker = ('NODE_UNIQUE_ID' in process.env);
cluster.isMaster = (cluster.isWorker === false);


function Worker(options) {
  if (!(this instanceof Worker))
    return new Worker(options);

  EventEmitter.call(this);

  if (!util.isObject(options))
    options = {};

  this.suicide = undefined;
  this.state = options.state || 'none';
  this.id = options.id | 0;

  if (options.process) {
    this.process = options.process;
    this.process.on('error', this.emit.bind(this, 'error'));
    this.process.on('message', this.emit.bind(this, 'message'));
  }
}
util.inherits(Worker, EventEmitter);

Worker.prototype.kill = function() {
  this.destroy.apply(this, arguments);
};

Worker.prototype.send = function() {
  this.process.send.apply(this.process, arguments);
};

Worker.prototype.isDead = function isDead() {
  return this.process.exitCode != null || this.process.signalCode != null;
};

Worker.prototype.isConnected = function isConnected() {
  return this.process.connected;
};

// Master/worker specific methods are defined in the *Init() functions.

function SharedHandle(key, address, port, addressType, backlog, fd) {
  this.key = key;
  this.workers = [];
  this.handle = null;
  this.errno = 0;

  // FIXME(bnoordhuis) Polymorphic return type for lack of a better solution.
  var rval;
  if (addressType === 'udp4' || addressType === 'udp6')
    rval = dgram._createSocketHandle(address, port, addressType, fd);
  else
    rval = net._createServerHandle(address, port, addressType, fd);

  if (util.isNumber(rval))
    this.errno = rval;
  else
    this.handle = rval;
}

SharedHandle.prototype.add = function(worker, send) {
  assert(this.workers.indexOf(worker) === -1);
  this.workers.push(worker);
  send(this.errno, null, this.handle);
};

SharedHandle.prototype.remove = function(worker) {
  var index = this.workers.indexOf(worker);
  if (index === -1) return false; // The worker wasn't sharing this handle.
  this.workers.splice(index, 1);
  if (this.workers.length !== 0) return false;
  this.handle.close();
  this.handle = null;
  return true;
};


// Start a round-robin server. Master accepts connections and distributes
// them over the workers.
function RoundRobinHandle(key, address, port, addressType, backlog, fd) {
  this.key = key;
  this.all = {};
  this.free = [];
  this.handles = [];
  this.handle = null;
  this.server = net.createServer(assert.fail);

  if (fd >= 0)
    this.server.listen({ fd: fd });
  else if (port >= 0)
    this.server.listen(port, address);
  else
    this.server.listen(address);  // UNIX socket path.

  var self = this;
  this.server.once('listening', function() {
    self.handle = self.server._handle;
    self.handle.onconnection = self.distribute.bind(self);
    self.server._handle = null;
    self.server = null;
  });
}

RoundRobinHandle.prototype.add = function(worker, send) {
  assert(worker.id in this.all === false);
  this.all[worker.id] = worker;

  var self = this;
  function done() {
    if (self.handle.getsockname) {
      var out = {};
      var err = self.handle.getsockname(out);
      // TODO(bnoordhuis) Check err.
      send(null, { sockname: out }, null);
    }
    else {
      send(null, null, null);  // UNIX socket.
    }
    self.handoff(worker);  // In case there are connections pending.
  }

  if (util.isNull(this.server)) return done();
  // Still busy binding.
  this.server.once('listening', done);
  this.server.once('error', function(err) {
    // Hack: translate 'EADDRINUSE' error string back to numeric error code.
    // It works but ideally we'd have some backchannel between the net and
    // cluster modules for stuff like this.
    var errno = process.binding('uv')['UV_' + err.errno];
    send(errno, null);
  });
};

RoundRobinHandle.prototype.remove = function(worker) {
  if (worker.id in this.all === false) return false;
  delete this.all[worker.id];
  var index = this.free.indexOf(worker);
  if (index !== -1) this.free.splice(index, 1);
  if (Object.getOwnPropertyNames(this.all).length !== 0) return false;
  for (var handle; handle = this.handles.shift(); handle.close());
  this.handle.close();
  this.handle = null;
  return true;
};

RoundRobinHandle.prototype.distribute = function(err, handle) {
  this.handles.push(handle);
  var worker = this.free.shift();
  if (worker) this.handoff(worker);
};

RoundRobinHandle.prototype.handoff = function(worker) {
  if (worker.id in this.all === false) {
    return;  // Worker is closing (or has closed) the server.
  }
  var handle = this.handles.shift();
  if (util.isUndefined(handle)) {
    this.free.push(worker);  // Add to ready queue again.
    return;
  }
  var message = { act: 'newconn', key: this.key };
  var self = this;
  sendHelper(worker.process, message, handle, function(reply) {
    if (reply.accepted)
      handle.close();
    else
      self.distribute(0, handle);  // Worker is shutting down. Send to another.
    self.handoff(worker);
  });
};


if (cluster.isMaster)
  masterInit();
else
  workerInit();

function masterInit() {
  cluster.workers = {};

  var intercom = new EventEmitter;
  cluster.settings = {};

  // XXX(bnoordhuis) Fold cluster.schedulingPolicy into cluster.settings?
  var schedulingPolicy = {
    'none': SCHED_NONE,
    'rr': SCHED_RR
  }[process.env.NODE_CLUSTER_SCHED_POLICY];

  if (util.isUndefined(schedulingPolicy)) {
    // FIXME Round-robin doesn't perform well on Windows right now due to the
    // way IOCP is wired up. Bert is going to fix that, eventually.
    schedulingPolicy = (process.platform === 'win32') ? SCHED_NONE : SCHED_RR;
  }

  cluster.schedulingPolicy = schedulingPolicy;
  cluster.SCHED_NONE = SCHED_NONE;  // Leave it to the operating system.
  cluster.SCHED_RR = SCHED_RR;      // Master distributes connections.

  // Keyed on address:port:etc. When a worker dies, we walk over the handles
  // and remove() the worker from each one. remove() may do a linear scan
  // itself so we might end up with an O(n*m) operation. Ergo, FIXME.
  var handles = {};

  var initialized = false;
  cluster.setupMaster = function(options) {
    var settings = {
      args: process.argv.slice(2),
      exec: process.argv[1],
      execArgv: process.execArgv,
      silent: false
    };
    settings = util._extend(settings, cluster.settings);
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
    if (initialized === true)
      return process.nextTick(function() {
        cluster.emit('setup', settings);
      });
    initialized = true;
    schedulingPolicy = cluster.schedulingPolicy;  // Freeze policy.
    assert(schedulingPolicy === SCHED_NONE || schedulingPolicy === SCHED_RR,
           'Bad cluster.schedulingPolicy: ' + schedulingPolicy);

    var hasDebugArg = process.execArgv.some(function(argv) {
      return /^(--debug|--debug-brk)(=\d+)?$/.test(argv);
    });

    process.nextTick(function() {
      cluster.emit('setup', settings);
    });

    // Send debug signal only if not started in debug mode, this helps a lot
    // on windows, because RegisterDebugHandler is not called when node starts
    // with --debug.* arg.
    if (hasDebugArg)
      return;

    process.on('internalMessage', function(message) {
      if (message.cmd !== 'NODE_DEBUG_ENABLED') return;
      var key;
      for (key in cluster.workers) {
        var worker = cluster.workers[key];
        if (worker.state === 'online' || worker.state === 'listening') {
          process._debugProcess(worker.process.pid);
        } else {
          worker.once('online', function() {
            process._debugProcess(this.process.pid);
          });
        }
      }
    });
  };

  function createWorkerProcess(id, env) {
    var workerEnv = util._extend({}, process.env);
    var execArgv = cluster.settings.execArgv.slice();
    var debugPort = process.debugPort + id;
    var hasDebugArg = false;

    workerEnv = util._extend(workerEnv, env);
    workerEnv.NODE_UNIQUE_ID = '' + id;

    for (var i = 0; i < execArgv.length; i++) {
      var match = execArgv[i].match(/^(--debug|--debug-brk)(=\d+)?$/);

      if (match) {
        execArgv[i] = match[1] + '=' + debugPort;
        hasDebugArg = true;
      }
    }

    if (!hasDebugArg)
      execArgv = ['--debug-port=' + debugPort].concat(execArgv);

    return fork(cluster.settings.exec, cluster.settings.args, {
      env: workerEnv,
      silent: cluster.settings.silent,
      execArgv: execArgv,
      gid: cluster.settings.gid,
      uid: cluster.settings.uid
    });
  }

  var ids = 0;

  cluster.fork = function(env) {
    cluster.setupMaster();
    var id = ++ids;
    var workerProcess = createWorkerProcess(id, env);
    var worker = new Worker({
      id: id,
      process: workerProcess
    });

    function removeWorker(worker) {
      assert(worker);

      delete cluster.workers[worker.id];

      if (Object.keys(cluster.workers).length === 0) {
        assert(Object.keys(handles).length === 0, 'Resource leak detected.');
        intercom.emit('disconnect');
      }
    }

    function removeHandlesForWorker(worker) {
      assert(worker);

      for (var key in handles) {
        var handle = handles[key];
        if (handle.remove(worker)) delete handles[key];
      }
    }

    worker.process.once('exit', function(exitCode, signalCode) {
      /*
       * Remove the worker from the workers list only
       * if it has disconnected, otherwise we might
       * still want to access it.
       */
      if (!worker.isConnected()) removeWorker(worker);

      worker.suicide = !!worker.suicide;
      worker.state = 'dead';
      worker.emit('exit', exitCode, signalCode);
      cluster.emit('exit', worker, exitCode, signalCode);
    });

    worker.process.once('disconnect', function() {
      /*
       * Now is a good time to remove the handles
       * associated with this worker because it is
       * not connected to the master anymore.
       */
      removeHandlesForWorker(worker);

      /*
       * Remove the worker from the workers list only
       * if its process has exited. Otherwise, we might
       * still want to access it.
       */
      if (worker.isDead()) removeWorker(worker);

      worker.suicide = !!worker.suicide;
      worker.state = 'disconnected';
      worker.emit('disconnect');
      cluster.emit('disconnect', worker);
    });

    worker.process.on('internalMessage', internal(worker, onmessage));
    process.nextTick(function() {
      cluster.emit('fork', worker);
    });
    cluster.workers[worker.id] = worker;
    return worker;
  };

  cluster.disconnect = function(cb) {
    var workers = Object.keys(cluster.workers);
    if (workers.length === 0) {
      process.nextTick(intercom.emit.bind(intercom, 'disconnect'));
    } else {
      for (var key in workers) {
        key = workers[key];
        if (cluster.workers[key].isConnected())
          cluster.workers[key].disconnect();
      }
    }
    if (cb) intercom.once('disconnect', cb);
  };

  Worker.prototype.disconnect = function() {
    this.suicide = true;
    send(this, { act: 'disconnect' });
  };

  Worker.prototype.destroy = function(signo) {
    signo = signo || 'SIGTERM';
    var proc = this.process;
    if (this.isConnected()) {
      this.once('disconnect', proc.kill.bind(proc, signo));
      this.disconnect();
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
    else if (message.act === 'close')
      close(worker, message);
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
    var handle = handles[key];
    if (util.isUndefined(handle)) {
      var constructor = RoundRobinHandle;
      // UDP is exempt from round-robin connection balancing for what should
      // be obvious reasons: it's connectionless. There is nothing to send to
      // the workers except raw datagrams and that's pointless.
      if (schedulingPolicy !== SCHED_RR ||
          message.addressType === 'udp4' ||
          message.addressType === 'udp6') {
        constructor = SharedHandle;
      }
      handles[key] = handle = new constructor(key,
                                              message.address,
                                              message.port,
                                              message.addressType,
                                              message.backlog,
                                              message.fd);
    }
    if (!handle.data) handle.data = message.data;

    // Set custom server data
    handle.add(worker, function(errno, reply, handle) {
      reply = util._extend({
        errno: errno,
        key: key,
        ack: message.seq,
        data: handles[key].data
      }, reply);
      if (errno) delete handles[key];  // Gives other workers a chance to retry.
      send(worker, reply, handle);
    });
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

  // Round-robin only. Server in worker is closing, remove from list.
  function close(worker, message) {
    var key = message.key;
    var handle = handles[key];
    if (handle.remove(worker)) delete handles[key];
  }

  function send(worker, message, handle, cb) {
    sendHelper(worker.process, message, handle, cb);
  }
}


function workerInit() {
  var handles = {};

  // Called from src/node.js
  cluster._setupWorker = function() {
    var worker = new Worker({
      id: +process.env.NODE_UNIQUE_ID | 0,
      process: process,
      state: 'online'
    });
    cluster.worker = worker;
    process.once('disconnect', function() {
      if (!worker.suicide) {
        // Unexpected disconnect, master exited, or some such nastiness, so
        // worker exits immediately.
        process.exit(0);
      }
    });
    process.on('internalMessage', internal(worker, onmessage));
    send({ act: 'online' });
    function onmessage(message, handle) {
      if (message.act === 'newconn')
        onconnection(message, handle);
      else if (message.act === 'disconnect')
        worker.disconnect();
    }
  };

  // obj is a net#Server or a dgram#Socket object.
  cluster._getServer = function(obj, address, port, addressType, fd, cb) {
    var message = {
      addressType: addressType,
      address: address,
      port: port,
      act: 'queryServer',
      fd: fd,
      data: null
    };
    // Set custom data on handle (i.e. tls tickets key)
    if (obj._getServerData) message.data = obj._getServerData();
    send(message, function(reply, handle) {
      if (obj._setServerData) obj._setServerData(reply.data);

      if (handle)
        shared(reply, handle, cb);  // Shared listen socket.
      else
        rr(reply, cb);              // Round-robin.
    });
    obj.once('listening', function() {
      cluster.worker.state = 'listening';
      var address = obj.address();
      message.act = 'listening';
      message.port = address && address.port || port;
      send(message);
    });
  };

  // Shared listen socket.
  function shared(message, handle, cb) {
    var key = message.key;
    // Monkey-patch the close() method so we can keep track of when it's
    // closed. Avoids resource leaks when the handle is short-lived.
    var close = handle.close;
    handle.close = function() {
      delete handles[key];
      return close.apply(this, arguments);
    };
    assert(util.isUndefined(handles[key]));
    handles[key] = handle;
    cb(message.errno, handle);
  }

  // Round-robin. Master distributes handles across workers.
  function rr(message, cb) {
    if (message.errno)
      return cb(message.errno, null);

    var key = message.key;
    function listen(backlog) {
      // TODO(bnoordhuis) Send a message to the master that tells it to
      // update the backlog size. The actual backlog should probably be
      // the largest requested size by any worker.
      return 0;
    }

    function close() {
      // lib/net.js treats server._handle.close() as effectively synchronous.
      // That means there is a time window between the call to close() and
      // the ack by the master process in which we can still receive handles.
      // onconnection() below handles that by sending those handles back to
      // the master.
      if (util.isUndefined(key)) return;
      send({ act: 'close', key: key });
      delete handles[key];
      key = undefined;
    }

    function getsockname(out) {
      if (key) util._extend(out, message.sockname);
      return 0;
    }

    // Faux handle. Mimics a TCPWrap with just enough fidelity to get away
    // with it. Fools net.Server into thinking that it's backed by a real
    // handle.
    var handle = {
      close: close,
      listen: listen
    };
    if (message.sockname) {
      handle.getsockname = getsockname;  // TCP handles only.
    }
    assert(util.isUndefined(handles[key]));
    handles[key] = handle;
    cb(0, handle);
  }

  // Round-robin connection.
  function onconnection(message, handle) {
    var key = message.key;
    var server = handles[key];
    var accepted = !util.isUndefined(server);
    send({ ack: message.seq, accepted: accepted });
    if (accepted) server.onconnection(0, handle);
  }

  Worker.prototype.disconnect = function() {
    this.suicide = true;
    for (var key in handles) {
      var handle = handles[key];
      delete handles[key];
      handle.close();
    }
    process.disconnect();
  };

  Worker.prototype.destroy = function() {
    this.suicide = true;
    if (!this.isConnected()) process.exit(0);
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
    'use strict';

    if (message.cmd !== 'NODE_CLUSTER') return;
    var fn = cb;
    if (!util.isUndefined(message.ack)) {
      fn = callbacks[message.ack];
      delete callbacks[message.ack];
    }
    fn.apply(worker, arguments);
  };
}
