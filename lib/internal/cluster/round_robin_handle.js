'use strict';

const {
  ArrayIsArray,
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

function RoundRobinHandle(key, address, { port, fd, flags, backlog }) {
  this.key = key;
  this.all = new SafeMap();
  this.free = new SafeMap();
  this.handles = init(ObjectCreate(null));
  this.handle = null;
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
  assert(this.all.has(worker.id) === false);
  this.all.set(worker.id, worker);

  const done = () => {
    if (this.handle.getsockname) {
      const out = {};
      this.handle.getsockname(out);
      // TODO(bnoordhuis) Check err.
      send(null, { sockname: out }, null);
    } else {
      send(null, null, null);  // UNIX socket.
    }

    this.handoff(worker);  // In case there are connections pending.
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
  const existed = this.all.delete(worker.id);

  if (!existed)
    return false;

  this.free.delete(worker.id);

  if (this.all.size !== 0)
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
  // eslint-disable-next-line node-core/no-array-destructuring
  const [ workerEntry ] = this.free; // this.free is a SafeMap

  if (ArrayIsArray(workerEntry)) {
    const { 0: workerId, 1: worker } = workerEntry;
    this.free.delete(workerId);
    this.handoff(worker);
  }
};

RoundRobinHandle.prototype.handoff = function(worker) {
  if (!this.all.has(worker.id)) {
    return;  // Worker is closing (or has closed) the server.
  }

  const handle = peek(this.handles);

  if (handle === null) {
    this.free.set(worker.id, worker);  // Add to ready queue again.
    return;
  }

  remove(handle);

  const message = { act: 'newconn', key: this.key };

  sendHelper(worker.process, message, handle, (reply) => {
    if (reply.accepted)
      handle.close();
    else
      this.distribute(0, handle);  // Worker is shutting down. Send to another.

    this.handoff(worker);
  });
};
