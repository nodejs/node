'use strict';

const {
  Boolean,
  Map,
} = primordials;

const assert = require('internal/assert');
const net = require('net');
const { sendHelper } = require('internal/cluster/utils');
const { constants } = internalBinding('tcp_wrap');

class RoundRobinHandle {
  constructor(key, address, { port, fd, flags }) {
    this.key = key;
    this.all = new Map();
    this.free = new Map();
    this.handles = [];
    this.handle = null;
    this.server = net.createServer(assert.fail);
    this.attachListener(fd, port, address, flags); // UNIX socket path.
    this.server.once('listening', () => {
      this.handle = this.server._handle;
      this.handle.onconnection = (err, handle) => this.distribute(err, handle);
      this.server._handle = null;
      this.server = null;
    });
  }

  attachListener(fd, port, address, flags) {
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
      this.server.listen(address);
  }

  add(worker, send) {
    assert(!this.all.has(worker.id));
    this.all.set(worker.id, worker);
    const done = () => {
      if (this.handle.getsockname) {
        const out = {};
        this.handle.getsockname(out);
        // TODO(bnoordhuis) Check err.
        send(null, { sockname: out }, null);
      } else {
        send(null, null, null); // UNIX socket.
      }
      this.handoff(worker); // In case there are connections pending.
    };
    if (this.server === null)
      return done();
    // Still busy binding.
    this.server.once('listening', done);
    this.server.once('error', (err) => {
      send(err.errno, null);
    });
  }

  remove(worker) {
    const existed = this.all.delete(worker.id);
    if (!existed)
      return false;
    this.free.delete(worker.id);
    if (this.all.size !== 0)
      return false;
    for (const handle of this.handles) {
      handle.close();
    }
    this.handles = [];
    this.handle.close();
    this.handle = null;
    return true;
  }

  distribute(err, handle) {
    this.handles.push(handle);
    const workerId = this.free.keys().next().value;
    if (workerId) {
      const worker = this.free.get(workerId);
      this.free.delete(workerId);
      this.handoff(worker);
    }
  }

  handoff(worker) {
    if (this.all.has(worker.id) === false) {
      return; // Worker is closing (or has closed) the server.
    }
    const handle = this.handles.shift();
    if (handle === undefined) {
      this.free.set(worker.id, worker); // Add to ready queue again.
      return;
    }
    const message = { act: 'newconn', key: this.key };
    sendHelper(worker.process, message, handle, (reply) => {
      if (reply.accepted)
        handle.close();
      else
        this.distribute(0, handle); // Worker is shutting down. Send to another.
      this.handoff(worker);
    });
  }
}

module.exports = RoundRobinHandle;
