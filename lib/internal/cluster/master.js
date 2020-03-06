'use strict';

const {
  Map,
  ObjectKeys,
  ObjectValues,
} = primordials;

const assert = require('internal/assert');
const { fork } = require('child_process');
const path = require('path');
const EventEmitter = require('events');
const RoundRobinHandle = require('internal/cluster/round_robin_handle');
const SharedHandle = require('internal/cluster/shared_handle');
const Worker = require('internal/cluster/worker');
const { internal, sendHelper } = require('internal/cluster/utils');
const cluster = new EventEmitter();
const intercom = new EventEmitter();
const SCHED_NONE = 1;
const SCHED_RR = 2;
const [ minPort, maxPort ] = [ 1024, 65535 ];
const { validatePort } = require('internal/validators');

module.exports = cluster;

const handles = new Map();
cluster.isWorker = false;
cluster.isMaster = true;
cluster.Worker = Worker;
cluster.workers = {};
cluster.settings = {};
cluster.SCHED_NONE = SCHED_NONE;  // Leave it to the operating system.
cluster.SCHED_RR = SCHED_RR;      // Master distributes connections.

let ids = 0;
let debugPortOffset = 1;
let initialized = false;

// XXX(bnoordhuis) Fold cluster.schedulingPolicy into cluster.settings?
let schedulingPolicy = {
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
  const settings = {
    args: process.argv.slice(2),
    exec: process.argv[1],
    execArgv: process.execArgv,
    silent: false,
    ...cluster.settings,
    ...options
  };

  // Tell V8 to write profile data for each process to a separate file.
  // Without --logfile=v8-%p.log, everything ends up in a single, unusable
  // file. (Unusable because what V8 logs are memory addresses and each
  // process has its own memory mappings.)
  if (settings.execArgv.some((s) => s.startsWith('--prof')) &&
      !settings.execArgv.some((s) => s.startsWith('--logfile='))) {
    settings.execArgv = [...settings.execArgv, '--logfile=v8-%p.log'];
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

    for (const worker of ObjectValues(cluster.workers)) {
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
  const workerEnv = { ...process.env, ...env, NODE_UNIQUE_ID: `${id}` };
  const execArgv = [...cluster.settings.execArgv];
  const debugArgRegex = /--inspect(?:-brk|-port)?|--debug-port/;
  const nodeOptions = process.env.NODE_OPTIONS ?
    process.env.NODE_OPTIONS : '';

  if (execArgv.some((arg) => arg.match(debugArgRegex)) ||
      nodeOptions.match(debugArgRegex)) {
    let inspectPort;
    if ('inspectPort' in cluster.settings) {
      if (typeof cluster.settings.inspectPort === 'function')
        inspectPort = cluster.settings.inspectPort();
      else
        inspectPort = cluster.settings.inspectPort;

      validatePort(inspectPort);
    } else {
      inspectPort = process.debugPort + debugPortOffset;
      if (inspectPort > maxPort)
        inspectPort = inspectPort - maxPort + minPort - 1;
      debugPortOffset++;
    }

    execArgv.push(`--inspect-port=${inspectPort}`);
  }

  return fork(cluster.settings.exec, cluster.settings.args, {
    cwd: cluster.settings.cwd,
    env: workerEnv,
    serialization: cluster.settings.serialization,
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

  if (ObjectKeys(cluster.workers).length === 0) {
    assert(handles.size === 0, 'Resource leak detected.');
    intercom.emit('disconnect');
  }
}

function removeHandlesForWorker(worker) {
  assert(worker);

  handles.forEach((handle, key) => {
    if (handle.remove(worker))
      handles.delete(key);
  });
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
  const workers = ObjectKeys(cluster.workers);

  if (workers.length === 0) {
    process.nextTick(() => intercom.emit('disconnect'));
  } else {
    for (const worker of ObjectValues(cluster.workers)) {
      if (worker.isConnected()) {
        worker.disconnect();
      }
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

  const key = `${message.address}:${message.port}:${message.addressType}:` +
              `${message.fd}:${message.index}`;
  let handle = handles.get(key);

  if (handle === undefined) {
    let address = message.address;

    // Find shortest path for unix sockets because of the ~100 byte limit
    if (message.port < 0 && typeof address === 'string' &&
        process.platform !== 'win32') {

      address = path.relative(process.cwd(), address);

      if (message.address.length < address.length)
        address = message.address;
    }

    let constructor = RoundRobinHandle;
    // UDP is exempt from round-robin connection balancing for what should
    // be obvious reasons: it's connectionless. There is nothing to send to
    // the workers except raw datagrams and that's pointless.
    if (schedulingPolicy !== SCHED_RR ||
        message.addressType === 'udp4' ||
        message.addressType === 'udp6') {
      constructor = SharedHandle;
    }

    handle = new constructor(key,
                             address,
                             message.port,
                             message.addressType,
                             message.fd,
                             message.flags);
    handles.set(key, handle);
  }

  if (!handle.data)
    handle.data = message.data;

  // Set custom server data
  handle.add(worker, (errno, reply, handle) => {
    const { data } = handles.get(key);

    if (errno)
      handles.delete(key);  // Gives other workers a chance to retry.

    send(worker, {
      errno,
      key,
      ack: message.seq,
      data,
      ...reply
    }, handle);
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
  const handle = handles.get(key);

  if (handle && handle.remove(worker))
    handles.delete(key);
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
