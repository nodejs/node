'use strict';

const EventEmitter = require('events');
const assert = require('assert');
const dgram = require('dgram');
const fork = require('child_process').fork;
const net = require('net');
const util = require('util');
const SCHED_NONE = 1;
const SCHED_RR = 2;

const uv = process.binding('uv');

const cluster = new EventEmitter();
module.exports = cluster;
cluster.Worker = Worker;
cluster.isWorker = ('NODE_UNIQUE_ID' in process.env);
cluster.isMaster = (cluster.isWorker === false);


function Worker(options) {
  if (!(this instanceof Worker))
    return new Worker(options);

  EventEmitter.call(this);

  if (options === null || typeof options !== 'object')
    options = {};

  this.exitedAfterDisconnect = undefined;

  Object.defineProperty(this, 'suicide', {
    get: function() {
      // TODO: Print deprecation message.
      return this.exitedAfterDisconnect;
    },
    set: function(val) {
      // TODO: Print deprecation message.
      this.exitedAfterDisconnect = val;
    },
    enumerable: true
  });

  this.state = options.state || 'none';
  this.id = options.id | 0;

  if (options.process) {
    this.process = options.process;
    this.process.on('error', (code, signal) =>
      this.emit('error', code, signal)
    );
    this.process.on('message', (message, handle) =>
      this.emit('message', message, handle)
    );
  }
}
util.inherits(Worker, EventEmitter);

Worker.prototype.kill = function() {
  this.destroy.apply(this, arguments);
};

Worker.prototype.send = function() {
  return this.process.send.apply(this.process, arguments);
};

Worker.prototype.isDead = function isDead() {
  return this.process.exitCode != null || this.process.signalCode != null;
};

Worker.prototype.isConnected = function isConnected() {
  return this.process.connected;
};

// Master/worker specific methods are defined in the *Init() functions.

function SharedHandle(key, address, port, addressType, backlog, fd, flags) {
  this.key = key;
  this.workers = [];
  this.handle = null;
  this.errno = 0;

  // FIXME(bnoordhuis) Polymorphic return type for lack of a better solution.
  var rval;
  if (addressType === 'udp4' || addressType === 'udp6')
    rval = dgram._createSocketHandle(address, port, addressType, fd, flags);
  else
    rval = net._createServerHandle(address, port, addressType, fd);

  if (typeof rval === 'number')
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

  this.server.once('listening', () => {
    this.handle = this.server._handle;
    this.handle.onconnection = (err, handle) => this.distribute(err, handle);
    this.server._handle = null;
    this.server = null;
  });
}

RoundRobinHandle.prototype.add = function(worker, send) {
  assert(worker.id in this.all === false);
  this.all[worker.id] = worker;

  const done = () => {
    if (this.handle.getsockname) {
      var out = {};
      this.handle.getsockname(out);
      // TODO(bnoordhuis) Check err.
      send(null, { sockname: out }, null);
    } else {
      send(null, null, null);  // UNIX socket.
    }
    this.handoff(worker);  // In case there are connections pending.
  };

  if (this.server === null) return done();
  // Still busy binding.
  this.server.once('listening', done);
  this.server.once('error', function(err) {
    // Hack: translate 'EADDRINUSE' error string back to numeric error code.
    // It works but ideally we'd have some backchannel between the net and
    // cluster modules for stuff like this.
    var errno = uv['UV_' + err.errno];
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
  if (handle === undefined) {
    this.free.push(worker);  // Add to ready queue again.
    return;
  }
  var message = { act: 'newconn', key: this.key };

  sendHelper(worker.process, message, handle, (reply) => {
    if (reply.accepted)
      handle.close();
    else
      this.distribute(0, handle);  // Worker is shutting down. Send to another.
    this.handoff(worker);
  });
};


if (cluster.isMaster)
  masterInit();
else
  workerInit();

function masterInit() {
  cluster.workers = {};

  var intercom = new EventEmitter();
  cluster.settings = {};

  // XXX(bnoordhuis) Fold cluster.schedulingPolicy into cluster.settings?
  var schedulingPolicy = {
    'none': SCHED_NONE,
    'rr': SCHED_RR
  }[process.env.NODE_CLUSTER_SCHED_POLICY];

  if (schedulingPolicy === undefined) {
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
  const handles = require('internal/cluster').handles;

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
    if (settings.execArgv.some((s) => s.startsWith('--prof')) &&
        !settings.execArgv.some((s) => s.startsWith('--logfile='))) {
      settings.execArgv = settings.execArgv.concat(['--logfile=v8-%p.log']);
    }
    cluster.settings = settings;
    if (initialized === true)
      return process.nextTick(setupSettingsNT, settings);
    initialized = true;
    schedulingPolicy = cluster.schedulingPolicy;  // Freeze policy.
    assert(schedulingPolicy === SCHED_NONE || schedulingPolicy === SCHED_RR,
           'Bad cluster.schedulingPolicy: ' + schedulingPolicy);

    var hasDebugArg = process.execArgv.some(function(argv) {
      return /^(--debug|--debug-brk)(=\d+)?$/.test(argv);
    });

    process.nextTick(setupSettingsNT, settings);

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

  function setupSettingsNT(settings) {
    cluster.emit('setup', settings);
  }

  var debugPortOffset = 1;

  function createWorkerProcess(id, env) {
    var workerEnv = util._extend({}, process.env);
    var execArgv = cluster.settings.execArgv.slice();
    var debugPort = 0;

    workerEnv = util._extend(workerEnv, env);
    workerEnv.NODE_UNIQUE_ID = '' + id;

    for (var i = 0; i < execArgv.length; i++) {
      var match = execArgv[i].match(
        /^(--inspect|--debug|--debug-(brk|port))(=\d+)?$/
      );

      if (match) {
        if (debugPort === 0) {
          debugPort = process.debugPort + debugPortOffset;
          ++debugPortOffset;
        }

        execArgv[i] = match[1] + '=' + debugPort;
      }
    }

    return fork(cluster.settings.exec, cluster.settings.args, {
      env: workerEnv,
      silent: cluster.settings.silent,
      execArgv: execArgv,
      stdio: cluster.settings.stdio,
      gid: cluster.settings.gid,
      uid: cluster.settings.uid
    });
  }

  var ids = 0;

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

  cluster.fork = function(env) {
    cluster.setupMaster();
    const id = ++ids;
    const workerProcess = createWorkerProcess(id, env);
    const worker = new Worker({
      id: id,
      process: workerProcess
    });

    worker.on('message', function(message, handle) {
      cluster.emit('message', this, message, handle);
    });

    worker.process.once('exit', function(exitCode, signalCode) {
      /*
       * Remove the worker from the workers list only
       * if it has disconnected, otherwise we might
       * still want to access it.
       */
      if (!worker.isConnected()) {
        removeHandlesForWorker(worker);
        removeWorker(worker);
      }

      worker.exitedAfterDisconnect = !!worker.exitedAfterDisconnect;
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

      worker.exitedAfterDisconnect = !!worker.exitedAfterDisconnect;
      worker.state = 'disconnected';
      worker.emit('disconnect');
      cluster.emit('disconnect', worker);
    });

    worker.process.on('internalMessage', internal(worker, onmessage));
    process.nextTick(emitForkNT, worker);
    cluster.workers[worker.id] = worker;
    return worker;
  };

  function emitForkNT(worker) {
    cluster.emit('fork', worker);
  }

  cluster.disconnect = function(cb) {
    var workers = Object.keys(cluster.workers);
    if (workers.length === 0) {
      process.nextTick(() => intercom.emit('disconnect'));
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
    this.exitedAfterDisconnect = true;
    send(this, { act: 'disconnect' });
    removeHandlesForWorker(this);
    removeWorker(this);
  };

  Worker.prototype.destroy = function(signo) {
    signo = signo || 'SIGTERM';
    var proc = this.process;
    if (this.isConnected()) {
      this.once('disconnect', () => proc.kill(signo));
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
    else if (message.act === 'exitedAfterDisconnect')
      exitedAfterDisconnect(worker, message);
    else if (message.act === 'close')
      close(worker, message);
  }

  function online(worker) {
    worker.state = 'online';
    worker.emit('online');
    cluster.emit('online', worker);
  }

  function exitedAfterDisconnect(worker, message) {
    worker.exitedAfterDisconnect = true;
    send(worker, { ack: message.seq });
  }

  function queryServer(worker, message) {
    // Stop processing if worker already disconnecting
    if (worker.exitedAfterDisconnect)
      return;
    var args = [message.address,
                message.port,
                message.addressType,
                message.fd,
                message.index];
    var key = args.join(':');
    var handle = handles[key];
    if (handle === undefined) {
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
                                              message.fd,
                                              message.flags);
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

  // Server in worker is closing, remove from list.  The handle may have been
  // removed by a prior call to removeHandlesForWorker() so guard against that.
  function close(worker, message) {
    var key = message.key;
    var handle = handles[key];
    if (handle && handle.remove(worker)) delete handles[key];
  }

  function send(worker, message, handle, cb) {
    return sendHelper(worker.process, message, handle, cb);
  }
}


function workerInit() {
  var handles = {};
  var indexes = {};

  // Called from src/node.js
  cluster._setupWorker = function() {
    var worker = new Worker({
      id: +process.env.NODE_UNIQUE_ID | 0,
      process: process,
      state: 'online'
    });
    cluster.worker = worker;
    process.once('disconnect', function() {
      worker.emit('disconnect');
      if (!worker.exitedAfterDisconnect) {
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
        _disconnect.call(worker, true);
    }
  };

  // obj is a net#Server or a dgram#Socket object.
  cluster._getServer = function(obj, options, cb) {
    const indexesKey = [ options.address,
                         options.port,
                         options.addressType,
                         options.fd ].join(':');
    if (indexes[indexesKey] === undefined)
      indexes[indexesKey] = 0;
    else
      indexes[indexesKey]++;

    const message = util._extend({
      act: 'queryServer',
      index: indexes[indexesKey],
      data: null
    }, options);

    // Set custom data on handle (i.e. tls tickets key)
    if (obj._getServerData) message.data = obj._getServerData();
    send(message, function(reply, handle) {
      if (obj._setServerData) obj._setServerData(reply.data);

      if (handle)
        shared(reply, handle, indexesKey, cb);  // Shared listen socket.
      else
        rr(reply, indexesKey, cb);              // Round-robin.
    });
    obj.once('listening', function() {
      cluster.worker.state = 'listening';
      const address = obj.address();
      message.act = 'listening';
      message.port = address && address.port || options.port;
      send(message);
    });
  };

  // Shared listen socket.
  function shared(message, handle, indexesKey, cb) {
    var key = message.key;
    // Monkey-patch the close() method so we can keep track of when it's
    // closed. Avoids resource leaks when the handle is short-lived.
    var close = handle.close;
    handle.close = function() {
      send({ act: 'close', key: key });
      delete handles[key];
      delete indexes[indexesKey];
      return close.apply(this, arguments);
    };
    assert(handles[key] === undefined);
    handles[key] = handle;
    cb(message.errno, handle);
  }

  // Round-robin. Master distributes handles across workers.
  function rr(message, indexesKey, cb) {
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
      if (key === undefined) return;
      send({ act: 'close', key: key });
      delete handles[key];
      delete indexes[indexesKey];
      key = undefined;
    }

    function getsockname(out) {
      if (key) util._extend(out, message.sockname);
      return 0;
    }

    // XXX(bnoordhuis) Probably no point in implementing ref() and unref()
    // because the control channel is going to keep the worker alive anyway.
    function ref() {
    }

    function unref() {
    }

    // Faux handle. Mimics a TCPWrap with just enough fidelity to get away
    // with it. Fools net.Server into thinking that it's backed by a real
    // handle.
    var handle = {
      close: close,
      listen: listen,
      ref: ref,
      unref: unref,
    };
    if (message.sockname) {
      handle.getsockname = getsockname;  // TCP handles only.
    }
    assert(handles[key] === undefined);
    handles[key] = handle;
    cb(0, handle);
  }

  // Round-robin connection.
  function onconnection(message, handle) {
    var key = message.key;
    var server = handles[key];
    var accepted = server !== undefined;
    send({ ack: message.seq, accepted: accepted });
    if (accepted) server.onconnection(0, handle);
  }

  Worker.prototype.disconnect = function() {
    _disconnect.call(this);
  };

  Worker.prototype.destroy = function() {
    this.exitedAfterDisconnect = true;
    if (!this.isConnected()) {
      process.exit(0);
    } else {
      send({ act: 'exitedAfterDisconnect' }, () => process.disconnect());
      process.once('disconnect', () => process.exit(0));
    }
  };

  function send(message, cb) {
    return sendHelper(process, message, null, cb);
  }

  function _disconnect(masterInitiated) {
    this.exitedAfterDisconnect = true;
    let waitingCount = 1;

    function checkWaitingCount() {
      waitingCount--;
      if (waitingCount === 0) {
        // If disconnect is worker initiated, wait for ack to be sure
        // exitedAfterDisconnect is properly set in the master, otherwise, if
        // it's master initiated there's no need to send the
        // exitedAfterDisconnect message
        if (masterInitiated) {
          process.disconnect();
        } else {
          send({ act: 'exitedAfterDisconnect' }, () => process.disconnect());
        }
      }
    }

    for (const key in handles) {
      const handle = handles[key];
      delete handles[key];
      waitingCount++;

      if (handle.owner)
        handle.owner.close(checkWaitingCount);
      else
        handle.close(checkWaitingCount);
    }

    checkWaitingCount();
  }
}


var seq = 0;
var callbacks = {};
function sendHelper(proc, message, handle, cb) {
  if (!proc.connected)
    return false;

  // Mark message as internal. See INTERNAL_PREFIX in lib/child_process.js
  message = util._extend({ cmd: 'NODE_CLUSTER' }, message);
  if (cb) callbacks[seq] = cb;
  message.seq = seq;
  seq += 1;
  return proc.send(message, handle);
}


// Returns an internalMessage listener that hands off normal messages
// to the callback but intercepts and redirects ACK messages.
function internal(worker, cb) {
  return function(message, handle) {
    if (message.cmd !== 'NODE_CLUSTER') return;
    var fn = cb;
    if (message.ack !== undefined && callbacks[message.ack] !== undefined) {
      fn = callbacks[message.ack];
      delete callbacks[message.ack];
    }
    fn.apply(worker, arguments);
  };
}
