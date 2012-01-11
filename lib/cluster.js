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

var assert = require('assert');
var fork = require('child_process').fork;
var net = require('net');
var EventEmitter = require('events').EventEmitter;
var util = require('util');

function isObject(o) {
  return (typeof o === 'object' && o !== null);
}

function extendObject(origin, add) {
  // Don't do anything if add isn't an object
  if (!add) return origin;

  var keys = Object.keys(add),
      i = keys.length;
  while (i--) {
    origin[keys[i]] = add[keys[i]];
  }
  return origin;
}

var debug;
if (process.env.NODE_DEBUG && /cluster/.test(process.env.NODE_DEBUG)) {
  debug = function(x) {
    var prefix = process.pid + ',' +
        (process.env.NODE_WORKER_ID ? 'Worker' : 'Master');
    console.error(prefix, x);
  };
} else {
  debug = function() { };
}

// cluster object:
function cluster() {}
util.inherits(cluster, EventEmitter);
var cluster = module.exports = new cluster();

// Used in the master:
var masterStarted = false;
var ids = 0;
var serverHandlers = {};
var workerFilename;
var workerArgs;

// Used in the worker:
var serverLisenters = {};
var queryIds = 0;
var queryCallbacks = {};

// Define isWorker and isMaster
cluster.isWorker = 'NODE_UNIQUE_ID' in process.env;
cluster.isMaster = ! cluster.isWorker;

// The worker object is only used in a worker
cluster.worker = cluster.isWorker ? {} : null;
// The workers array is oly used in the naster
cluster.workers = cluster.isMaster ? {} : null;

// Simple function there call a function on each worker
function eachWorker(cb) {
  // Go througe all workers
  for (var id in cluster.workers) {
    if (cluster.workers.hasOwnProperty(id)) {
      cb(cluster.workers[id]);
    }
  }
}

// Call this from the master process. It will start child workers.
//
// options.workerFilename
// Specifies the script to execute for the child processes. Default is
// process.argv[1]
//
// options.args
// Specifies program arguments for the workers. The Default is
// process.argv.slice(2)
//
// options.workers
// The number of workers to start. Defaults to os.cpus().length.
function startMaster() {
  // This can only be called from the master.
  assert(cluster.isMaster);

  if (masterStarted) return;
  masterStarted = true;

  workerFilename = process.argv[1];
  workerArgs = process.argv.slice(2);

  process.on('uncaughtException', function(e) {
    console.error('Exception in cluster master process: ' +
        e.message + '\n' + e.stack);

    quickDestroyCluster();
    process.exit(1);
  });
}

// Check if a message is internal only
var INTERNAL_PREFIX = 'NODE_CLUSTER_';
function isInternalMessage(message) {
  return (isObject(message) &&
          typeof message.cmd === 'string' &&
          message.cmd.indexOf(INTERNAL_PREFIX) === 0);
}

// Modyfi message object to be internal
function internalMessage(inMessage) {
  var outMessage = extendObject({}, inMessage);

  // Add internal prefix to cmd
  outMessage.cmd = INTERNAL_PREFIX + (outMessage.cmd || '');

  return outMessage;
}

// Handle callback messges
function handleResponse(outMessage, outHandle, inMessage, inHandle, worker) {

  // The message there will be send
  var message = internalMessage(outMessage);

  // callback id - will be undefined if not set
  message._queryEcho = inMessage._requestEcho;

  // Call callback if a query echo is received
  if (inMessage._queryEcho) {
    queryCallbacks[inMessage._queryEcho](inMessage.content, inHandle);
    delete queryCallbacks[inMessage._queryEcho];
  }

  // Send if outWrap do contain something useful
  if (!(outMessage === undefined && message._queryEcho === undefined)) {
    sendInternalMessage(worker, message, outHandle);
  }
}

// Handle messages from both master and workers
var messageHandingObject = {};
function handleMessage(inMessage, inHandle, worker) {

  //Remove internal prefix
  var message = extendObject({}, inMessage);
  message.cmd = inMessage.cmd.substr(INTERNAL_PREFIX.length);

  var respondUsed = false;
  var respond = function(outMessage, outHandler) {
    respondUsed = true;
    handleResponse(outMessage, outHandler, inMessage, inHandle, worker);
  };

  // Run handler if it exist
  if (messageHandingObject[message.cmd]) {
    messageHandingObject[message.cmd](message, worker, respond);
  }

  // Send respond if it wasn't done
  if (respondUsed === false) {
    respond();
  }
}

// Messages to the master will be handled using this methods
if (cluster.isMaster) {

  // Handle online messages from workers
  messageHandingObject.online = function(message, worker) {
    worker.state = 'online';
    debug('Worker ' + worker.process.pid + ' online');
    worker.emit('online', worker);
    cluster.emit('online', worker);
  };

  // Handle queryServer messages form workers
  messageHandingObject.queryServer = function(message, worker, send) {

    // This sequence of infomation is unique to the connection but not
    // to the worker
    var args = [message.address, message.port, message.addressType];
    var key = args.join(':');
    var handler;

    if (serverHandlers.hasOwnProperty(key)) {
      handler = serverHandlers[key];
    } else {
      handler = serverHandlers[key] = net._createServerHandle.apply(net, args);
    }

    // echo callback with the fd handler associated with it
    send({}, handler);
  };

  // Handle listening messages from workers
  messageHandingObject.listening = function(message, worker) {

    worker.state = 'listening';

    // Emit listining, now that we know the worker is listning
    worker.emit('listening', worker, {
      address: message.address,
      port: message.port,
      addressType: message.addressType
    });
    cluster.emit('listening', worker, {
      address: message.address,
      port: message.port,
      addressType: message.addressType
    });
  };

  // Handle suicide messages from workers
  messageHandingObject.suicide = function(message, worker) {
    worker.suicide = true;
  };

}

// Messages to a worker will be handled using this methods
else if (cluster.isWorker) {

  // TODO: the disconnect step will use this
}

function toDecInt(value) {
  value = parseInt(value, 10);
  return isNaN(value) ? null : value;
}

// Create a worker object, there works both for master and worker
function Worker(customEnv) {
  if (!(this instanceof Worker)) return new Worker();

  var self = this;
  var env = process.env;

  // Assign uniqueID, default null
  this.uniqueID = cluster.isMaster ? ++ids : toDecInt(env.NODE_UNIQUE_ID);

  // Assign state
  this.state = 'none';

  // Create or get process
  if (cluster.isMaster) {

    // Create env object
    // first: copy and add uniqueID
    var envCopy = extendObject({}, env);
    envCopy['NODE_UNIQUE_ID'] = this.uniqueID;
    // second: extend envCopy with the env argument
    if (isObject(customEnv)) {
      envCopy = extendObject(envCopy, customEnv);
    }

    // fork worker
    this.process = fork(workerFilename, workerArgs, {
      'env': envCopy
    });

  } else {
    this.process = process;
  }

  if (cluster.isMaster) {
    // Save worker in the cluster.workers array
    cluster.workers[this.uniqueID] = this;

    // Emit a fork event, on next tick
    // There is no worker.fork event since this has no real purpose
    process.nextTick(function() {
      cluster.emit('fork', self);
    });
  }

  // Internal message: handle message
  this.process.on('internalMessage', function(message, handle) {
    debug('recived: ', message);

    // relay to handleMessage
    handleMessage(message, handle, self);
    return;
  });

  // Non-internal message: relay to Worker object
  this.process.on('message', function(message, handle) {
    self.emit('message', message, handle);
  });

  // Handle exit
  self.process.on('exit', function() {
    debug('worker id=' + self.uniqueID + ' died');

    // Prepare worker to die and emit events
    prepareDeath(self, 'dead', 'death');
  });

}
util.inherits(Worker, EventEmitter);
cluster.Worker = Worker;

function prepareDeath(worker, state, eventName) {

  // set state to disconnect
  worker.state = state;

  // Make suicide a boolean
  worker.suicide = !!worker.suicide;

  // Remove from workers in the master
  if (cluster.isMaster) {
    delete cluster.workers[worker.uniqueID];
  }

  // Emit events
  worker.emit(eventName, worker);
  cluster.emit(eventName, worker);
}

// Send internal message
function sendInternalMessage(worker, message/*, handler, callback*/) {

  // Exist callback
  var callback = arguments[arguments.length - 1];
  if (typeof callback !== 'function') {
    callback = undefined;
  }

  // exist handler
  var handler = arguments[2] !== callback ? arguments[2] : undefined;

  if (!isInternalMessage(message)) {
    message = internalMessage(message);
  }

  // Store callback for later
  if (callback) {
    message._requestEcho = worker.uniqueID + ':' + (++queryIds);
    queryCallbacks[message._requestEcho] = callback;
  }


  worker.send(message, handler);
}

// Send message to worker or master
Worker.prototype.send = function() {

  //You could also just use process.send in a worker
  this.process.send.apply(this.process, arguments);
};


function closeWorkerChannel(worker, callback) {
  //Apparently the .close method is async, but do not have a callback
  worker.process._channel.close();
  worker.process._channel = null;
  process.nextTick(callback);
}

// Kill the worker without restarting
Worker.prototype.destroy = function() {
  var self = this;

  this.suicide = true;

  if (cluster.isMaster) {
    // Stop channel
    // this way the worker won't need to propagate suicide state to master
    closeWorkerChannel(this, function() {
      // Then kill worker
      self.process.kill();
    });

  } else {
    // Channel is open
    if (this.process._channel !== null) {

      // Inform master that is is suicide and then kill
      sendInternalMessage(this, {cmd: 'suicide'}, function() {
        // Kill worker
        process.exit(0);
      });

      // When master do a quickDestroy the channel is not necesarily closed
      // at the point this function runs. For that reason we need to keep
      // checking that the channel is still open, until a actually callback
      // from the master is resicved. Also we can't do a timeout and then
      // just kill, since we don't know if the quickDestroy function was called.
      setInterval(function() {
        if (self.process._channel === null) {
          process.exit(0);
        }
      }, 200);

    } else {
      process.exit(0);
    }
  }
};

// Fork a new worker
cluster.fork = function(env) {
  // This can only be called from the master.
  assert(cluster.isMaster);

  // Make sure that the master has been initalized
  startMaster();

  return (new cluster.Worker(env));
};

// Sync way to quickly kill all cluster workers
// However the workers may not die instantly
function quickDestroyCluster() {
  eachWorker(function(worker) {
    worker.process.kill();
  });
}

// Internal function. Called from src/node.js when worker process starts.
cluster._setupWorker = function() {
  // Get worker class
  var worker = cluster.worker = new Worker();

  // Tell master that the worker is online
  worker.state = 'online';
  sendInternalMessage(worker, { cmd: 'online' });
};

// Internal function. Called by lib/net.js when attempting to bind a server.
cluster._getServer = function(tcpSelf, address, port, addressType, cb) {
  // This can only be called from a worker.
  assert(cluster.isWorker);

  // Store tcp instance for later use
  var key = [address, port, addressType].join(':');
  serverLisenters[key] = tcpSelf;

  // Send a listening message to the master
  tcpSelf.once('listening', function() {
    cluster.worker.state = 'listening';
    sendInternalMessage(cluster.worker, {
      cmd: 'listening',
      address: address,
      port: port,
      addressType: addressType
    });
  });

  // Request the fd handler from the master process
  var message = {
    cmd: 'queryServer',
    address: address,
    port: port,
    addressType: addressType
  };

  // The callback will be stored until the master has responed
  sendInternalMessage(cluster.worker, message, function(msg, handle) {
    cb(handle);
  });

};
