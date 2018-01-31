'use strict';
const assert = require('assert');
const { fork } = require('child_process');
const util = require('util');
const path = require('path');
const EventEmitter = require('events');
const RoundRobinHandle = require('internal/cluster/round_robin_handle');
const SharedHandle = require('internal/cluster/shared_handle');
const Worker = require('internal/cluster/worker');
const { internal, sendHelper, handles } = require('internal/cluster/utils');
const keys = Object.keys;
const cluster = new EventEmitter();
const intercom = new EventEmitter();
const SCHED_NONE = 1;
const SCHED_RR = 2;
const { isLegalPort } = require('internal/net');

module.exports = cluster;

cluster.isWorker = false;
cluster.isMaster = true;
cluster.Worker = Worker;
cluster.workers = {};
cluster.settings = {};
cluster.SCHED_NONE = SCHED_NONE;  // Leave it to the operating system.
cluster.SCHED_RR = SCHED_RR;      // Master distributes connections.

var ids = 0;
var debugPortOffset = 1;
var initialized = false;

// XXX(bnoordhuis) Fold cluster.schedulingPolicy into cluster.settings?
var schedulingPolicy = {
  'none': SCHED_NONE,
  'rr': SCHED_RR
}[process.env.NODE_CLUSTER_SCHED_POLICY];

if (schedulingPolicy === undefined) {
  // FIXME Round-robin doesn't perform well on Windows right now due to the
  // way IOCP is wired up.
  schedulingPolicy = (process.platform === 'win32') ? SCHED_NONE : SCHED_RR;
}

cluster.schedulingPolicy = schedulingPolicy;

cluster.setupMaster = function(options) {
  var settings = {
    args: process.argv.slice(2),
    exec: process.argv[1],
    execArgv: process.execArgv,
    silent: false
  };
  util._extend(settings, cluster.settings);
  util._extend(settings, options || {});

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
         `Bad cluster.schedulingPolicy: ${schedulingPolicy}`);

  process.nextTick(setupSettingsNT, settings);

  process.on('internalMessage', (message) => {
    if (message.cmd !== 'NODE_DEBUG_ENABLED')
      return;

    var key;
    for (key in cluster.workers) {
      const worker = cluster.workers[key];

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

function createWorkerProcess(id, env) {
  const workerEnv = util._extend({}, process.env);
  const execArgv = cluster.settings.execArgv.slice();
  const debugArgRegex = /--inspect(?:-brk|-port)?|--debug-port/;

  util._extend(workerEnv, env);
  workerEnv.NODE_UNIQUE_ID = '' + id;

  if (execArgv.some((arg) => arg.match(debugArgRegex))) {
    let inspectPort;
    if ('inspectPort' in cluster.settings) {
      if (typeof cluster.settings.inspectPort === 'function')
        inspectPort = cluster.settings.inspectPort();
      else
        inspectPort = cluster.settings.inspectPort;

      if (!isLegalPort(inspectPort)) {
        throw new TypeError('cluster.settings.inspectPort' +
          ' is invalid');
      }
    } else {
      inspectPort = process.debugPort + debugPortOffset;
      debugPortOffset++;
    }

    execArgv.push(`--inspect-port=${inspectPort}`);
  }

  return fork(cluster.settings.exec, cluster.settings.args, {
    cwd: cluster.settings.cwd,
    env: workerEnv,
    silent: cluster.settings.silent,
    windowsHide: cluster.settings.windowsHide,
    execArgv: execArgv,
    stdio: cluster.settings.stdio,
    gid: cluster.settings.gid,
    uid: cluster.settings.uid
  });
}

function removeWorker(worker) {
  assert(worker);
  delete cluster.workers[worker.id];

  if (keys(cluster.workers).length === 0) {
    assert(keys(handles).length === 0, 'Resource leak detected.');
    intercom.emit('disconnect');
  }
}

function removeHandlesForWorker(worker) {
  assert(worker);

  for (var key in handles) {
    const handle = handles[key];

    if (handle.remove(worker))
      delete handles[key];
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

  worker.process.once('exit', (exitCode, signalCode) => {
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

  worker.process.once('disconnect', () => {
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
    if (worker.isDead())
      removeWorker(worker);

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
  const workers = keys(cluster.workers);

  if (workers.length === 0) {
    process.nextTick(() => intercom.emit('disconnect'));
  } else {
    for (var key in workers) {
      key = workers[key];

      if (cluster.workers[key].isConnected())
        cluster.workers[key].disconnect();
    }
  }

  if (typeof cb === 'function')
    intercom.once('disconnect', cb);
};

function onmessage(message, handle) {
  const worker = this;

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

  const args = [message.address,
                message.port,
                message.addressType,
                message.fd,
                message.index];
  const key = args.join(':');
  var handle = handles[key];

  if (handle === undefined) {
    let address = message.address;

    // Find shortest path for unix sockets because of the ~100 byte limit
    if (message.port < 0 && typeof address === 'string' &&
        process.platform !== 'win32') {

      address = path.relative(process.cwd(), address);

      if (message.address.length < address.length)
        address = message.address;
    }

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
                                            address,
                                            message.port,
                                            message.addressType,
                                            message.fd,
                                            message.flags);
  }

  if (!handle.data)
    handle.data = message.data;

  // Set custom server data
  handle.add(worker, (errno, reply, handle) => {
    reply = util._extend({
      errno: errno,
      key: key,
      ack: message.seq,
      data: handles[key].data
    }, reply);

    if (errno)
      delete handles[key];  // Gives other workers a chance to retry.

    send(worker, reply, handle);
  });
}

function listening(worker, message) {
  const info = {
    addressType: message.addressType,
    address: message.address,
    port: message.port,
    fd: message.fd
  };

  worker.state = 'listening';
  worker.emit('listening', info);
  cluster.emit('listening', worker, info);
}

// Server in worker is closing, remove from list. The handle may have been
// removed by a prior call to removeHandlesForWorker() so guard against that.
function close(worker, message) {
  const key = message.key;
  const handle = handles[key];

  if (handle && handle.remove(worker))
    delete handles[key];
}

function send(worker, message, handle, cb) {
  return sendHelper(worker.process, message, handle, cb);
}

// Extend generic Worker with methods specific to the master process.
Worker.prototype.disconnect = function() {
  this.exitedAfterDisconnect = true;
  send(this, { act: 'disconnect' });
  removeHandlesForWorker(this);
  removeWorker(this);
  return this;
};

Worker.prototype.destroy = function(signo) {
  const proc = this.process;

  signo = signo || 'SIGTERM';

  if (this.isConnected()) {
    this.once('disconnect', () => proc.kill(signo));
    this.disconnect();
    return;
  }

  proc.kill(signo);
};
