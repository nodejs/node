'use strict';

const {
  Boolean,
  ObjectCreate,
  SafeMap,
} = primordials;

const assert = require('internal/assert');
const net = require('net');
const { sendHelper } = require('internal/cluster/utils');
const { append, init, isEmpty, peek, remove } = require('internal/linkedlist');
const { constants } = internalBinding('tcp_wrap');

module.exports = RoundRobinHandle;

function RoundRobinHandle(key, address, { port, fd, flags, backlog, scheduler }) {
  this.key = key;
  this.workers = new SafeMap();
  this.handles = init(ObjectCreate(null));
  this.handle = null;
  this.scheduler = scheduler;
  this.server = net.createServer(assert.fail);

  if (fd >= 0)
    this.server.listen({ fd, backlog });
  else if (port >= 0) {
    this.server.listen({
      port,
      host: address,
      // Currently, net module only supports `ipv6Only` option in `flags`.
      ipv6Only: Boolean(flags & constants.UV_TCP_IPV6ONLY),
      backlog,
    });
  } else
    this.server.listen(address, backlog);  // UNIX socket path.

  this.server.once('listening', () => {
    this.handle = this.server._handle;
    this.handle.onconnection = (err, handle) => this.distribute(err, handle);
    this.server._handle = null;
    this.server = null;
  });
}

RoundRobinHandle.prototype.add = function(worker, send) {
  assert(this.workers.has(worker.id) === false);
  this.workers.set(worker.id, worker);

  const done = () => {
    if (this.handle.getsockname) {
      const out = {};
      this.handle.getsockname(out);
      // TODO(bnoordhuis) Check err.
      send(null, { sockname: out }, null);
    } else {
      send(null, null, null);  // UNIX socket.
    }

    this.handoff();  // In case there are connections pending.
  };

  if (this.server === null)
    return done();

  // Still busy binding.
  this.server.once('listening', done);
  this.server.once('error', (err) => {
    send(err.errno, null);
  });
};

RoundRobinHandle.prototype.remove = function(worker) {
  const existed = this.workers.delete(worker.id);

  if (!existed)
    return false;

  if (this.workers.size !== 0)
    return false;

  while (!isEmpty(this.handles)) {
    const handle = peek(this.handles);
    handle.close();
    remove(handle);
  }

  this.handle.close();
  this.handle = null;
  return true;
};

RoundRobinHandle.prototype.distribute = function(err, handle) {
  append(this.handles, handle);
  this.handoff();
};

RoundRobinHandle.scheduler = function(workers) {
  if (workers.size > 0) {
    const { 0: workerId, 1: worker } = workers.entries().next().value;
    workers.delete(workerId);
    workers.set(workerId, worker);
    return worker;
  }
};

RoundRobinHandle.prototype.handoff = function() {
  const handle = peek(this.handles);

  if (handle === null) {
    return;
  }

  let socket;
  if (this.scheduler.exposeSocket === true) {
    socket = new net.Socket({
      handle,
      readable: false,
      writable: false,
      pauseOnCreate: true
    });
  }

  const worker = this.scheduler.execute(this.workers, socket);
  if (typeof worker === 'undefined') {
    return;
  }

  remove(handle);

  const message = { act: 'newconn', key: this.key };

  sendHelper(worker.process, message, handle, (reply) => {
    if (reply.accepted)
      handle.close();
    else
      this.distribute(0, handle);  // Worker is shutting down. Send to another.

    this.handoff();
  });
};
