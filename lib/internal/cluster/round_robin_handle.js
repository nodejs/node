'use strict';
const assert = require('assert');
const net = require('net');
const { sendHelper } = require('internal/cluster/utils');
const create = Object.create;
const getOwnPropertyNames = Object.getOwnPropertyNames;
const uv = process.binding('uv');

module.exports = RoundRobinHandle;

function RoundRobinHandle(key, address, port, addrType, fd, flags, scheduler) {
  this.key = key;
  this.all = create(null);
  this.workers = [];
  this.handles = [];
  this.handle = null;
  this.scheduler = scheduler;
  this.server = net.createServer(assert.fail);

  if (fd >= 0)
    this.server.listen({ fd });
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

RoundRobinHandle.scheduler = function(workers) {
  const worker = workers.shift();

  if (worker === undefined)
    return;

  workers.push(worker);  // Add to the back of the ready queue.
  return worker;
};


RoundRobinHandle.prototype.add = function(worker, send) {
  assert(worker.id in this.all === false);
  this.all[worker.id] = worker;

  const done = () => {
    if (this.handle.getsockname) {
      const out = {};
      this.handle.getsockname(out);
      // TODO(bnoordhuis) Check err.
      send(null, { sockname: out }, null);
    } else {
      send(null, null, null);  // UNIX socket.
    }

    this.workers.push(worker);
    this.handoff();  // In case there are connections pending.
  };

  if (this.server === null)
    return done();

  // Still busy binding.
  this.server.once('listening', done);
  this.server.once('error', (err) => {
    // Hack: translate 'EADDRINUSE' error string back to numeric error code.
    // It works but ideally we'd have some backchannel between the net and
    // cluster modules for stuff like this.
    const errno = uv['UV_' + err.errno];
    send(errno, null);
  });
};

RoundRobinHandle.prototype.remove = function(worker) {
  if (worker.id in this.all === false)
    return false;

  delete this.all[worker.id];
  const index = this.workers.indexOf(worker);

  if (index !== -1)
    this.workers.splice(index, 1);

  if (getOwnPropertyNames(this.all).length !== 0)
    return false;

  for (var handle; handle = this.handles.shift(); handle.close())
    ;

  this.handle.close();
  this.handle = null;
  return true;
};

RoundRobinHandle.prototype.distribute = function(err, handle) {
  this.handles.push(handle);
  this.handoff();
};

RoundRobinHandle.prototype.handoff = function() {
  const handle = this.handles[0];
  var socket;

  // There are currently no requests to schedule.
  if (handle === undefined)
    return;

  if (this.scheduler.exposeSocket === true) {
    socket = new net.Socket({
      handle,
      readable: false,
      writable: false,
      pauseOnCreate: true
    });
  }

  const worker = this.scheduler(this.workers, socket);

  // An invalid worker was returned, or the worker is closing the server.
  if (worker === null || typeof worker !== 'object' ||
      worker.id in this.all === false) {
    return;
  }

  this.handles.shift(); // Successfully scheduled, so dequeue.

  const message = { act: 'newconn', key: this.key };

  sendHelper(worker.process, message, handle, (reply) => {
    if (reply.accepted) {
      handle.close();
    } else {
      // Worker is shutting down. Send the handle to another worker.
      this.distribute(0, handle);
    }

    this.handoff();
  });
};
