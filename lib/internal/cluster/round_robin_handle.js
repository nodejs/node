'use strict';
const assert = require('internal/assert');
const net = require('net');
const { sendHelper } = require('internal/cluster/utils');
const uv = internalBinding('uv');
const { constants } = internalBinding('tcp_wrap');

module.exports = RoundRobinHandle;

function RoundRobinHandle(key, address, port, addressType, fd, flags) {
  this.key = key;
  this.all = new Map();
  this.free = [];
  this.handles = [];
  this.handle = null;
  this.server = net.createServer(assert.fail);

  if (fd >= 0)
    this.server.listen({ fd });
  else if (port >= 0) {
    this.server.listen({
      port,
      host: address,
      // Currently, net module only supports `ipv6Only` option in `flags`.
      ipv6Only: Boolean(flags & constants.UV_TCP_IPV6ONLY),
    });
  } else
    this.server.listen(address);  // UNIX socket path.

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
    // Hack: translate 'EADDRINUSE' error string back to numeric error code.
    // It works but ideally we'd have some backchannel between the net and
    // cluster modules for stuff like this.
    send(uv[`UV_${err.errno}`], null);
  });
};

RoundRobinHandle.prototype.remove = function(worker) {
  const existed = this.all.delete(worker.id);

  if (!existed)
    return false;

  const index = this.free.indexOf(worker);

  if (index !== -1)
    this.free.splice(index, 1);

  if (this.all.size !== 0)
    return false;

  for (const handle of this.handles) {
    handle.close();
  }
  this.handles = [];

  this.handle.close();
  this.handle = null;
  return true;
};

RoundRobinHandle.prototype.distribute = function(err, handle) {
  this.handles.push(handle);
  const worker = this.free.shift();

  if (worker)
    this.handoff(worker);
};

RoundRobinHandle.prototype.handoff = function(worker) {
  if (this.all.has(worker.id) === false) {
    return;  // Worker is closing (or has closed) the server.
  }

  const handle = this.handles.shift();

  if (handle === undefined) {
    this.free.push(worker);  // Add to ready queue again.
    return;
  }

  const message = { act: 'newconn', key: this.key };

  sendHelper(worker.process, message, handle, (reply) => {
    if (reply.accepted)
      handle.close();
    else
      this.distribute(0, handle);  // Worker is shutting down. Send to another.

    this.handoff(worker);
  });
};
