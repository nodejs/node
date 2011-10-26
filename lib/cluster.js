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
var amMaster; // Used for asserts


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


// Used in the master:
var ids = 0;
var workers = [];
var servers = {};

// Used in the worker:
var workerId = 0;
var queryIds = 0;
var queryCallbacks = {};

exports.isWorker = 'NODE_WORKER_ID' in process.env;
exports.isMaster = ! exports.isWorker;

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
exports.startMaster = function(options) {
  amMaster = true;

  if (!options) {
    options = {};
  }

  if (!options.workerFilename) {
    options.workerFilename = process.argv[1];
  }

  if (!options.args) {
    options.args = process.argv.slice(2);
  }

  if (!options.workers) {
    options.workers = require('os').cpus().length;
  }

  for (var i = 0; i < options.workers; i++) {
    forkWorker(options.workerFilename, options.args);
  }

  process.on('uncaughtException', function(e) {
    // Quickly try to kill all the workers.
    // TODO: be session leader - will cause auto SIGHUP to the children.
    for (var id in workers) {
      if (workers[id]) {
        debug("kill worker " + id);
        workers[id].kill();
      }
    }

    console.error("Exception in cluster master process: " +
        e.message + '\n' + e.stack);
    console.error("Please report this bug.");
    process.exit(1);
  });
}


function handleWorkerMessage(worker, message) {
  assert.ok(amMaster);

  debug("recv " + JSON.stringify(message));

  switch (message.cmd) {
    case 'online': 
      console.log("Worker " + worker.pid + " online");
      workers[message._workerId] = worker;
      break;

    case 'queryServer':
      var key = message.address + ":" +
                message.port + ":" +
                message.addressType;
      var response = { _queryId: message._queryId };

      if (key in servers == false) {
        // Create a new server.
        debug('create new server ' + key);
        servers[key] = net._createServerHandle(message.address,
                                               message.port,
                                               message.addressType);
      }
      worker.send(response, servers[key]);
      break;

    default:
      // Ignore.
      break;
  }
}


function forkWorker(workerFilename, args) {
  var id = ++ids;
  var envCopy = {};

  for (var x in process.env) {
    envCopy[x] = process.env[x];
  }

  envCopy['NODE_WORKER_ID'] = id;

  var worker = fork(workerFilename, args, {
    env: envCopy
  });

  worker.on('message', function(message) {
    handleWorkerMessage(worker, message);
  });

  worker.on('exit', function() {
    debug('worker id=' + id + ' died');
    delete workers[id];
  });

  return worker;
}


exports.startWorker = function() {
  assert.ok(!amMaster);
  amMaster = false;
  workerId = parseInt(process.env.NODE_WORKER_ID);

  queryMaster({ cmd: 'online' });

  // Make callbacks from queryMaster()
  process.on('message', function(msg, handle) {
    debug("recv " + JSON.stringify(msg));
    if (msg._queryId && msg._queryId in queryCallbacks) {
      var cb = queryCallbacks[msg._queryId];
      if (typeof cb == 'function') {
        cb(msg, handle);
      }
      delete queryCallbacks[msg._queryId]
    }
  });
};


function queryMaster(msg, cb) {
  assert.ok(!amMaster);

  debug('send ' + JSON.stringify(msg));

  // Grab some random queryId
  msg._queryId = (++queryIds);
  msg._workerId = workerId;

  // Store callback for later. Callback called in startWorker.
  if (cb) {
    queryCallbacks[msg._queryId] = cb;
  }

  // Send message to master.
  process.send(msg);
}


exports.getServer = function(address, port, addressType, cb) {
  assert.ok(!amMaster);

  queryMaster({
    cmd: "queryServer",
    address: address,
    port: port,
    addressType: addressType
  }, function(msg, handle) {
    cb(handle);
  });
};
