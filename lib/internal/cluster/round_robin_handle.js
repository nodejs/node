'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  Boolean,
  SafeMap,
} = primordials;

const assert = require('internal/assert');
const net = require('net');
const { sendHelper } = require('internal/cluster/utils');
const { constants } = internalBinding('tcp_wrap');

class RoundRobinHandle {
  constructor(key, address, { port, fd, flags }) {
    this.key = key;
    this.all = new SafeMap();
    this.free = new SafeMap();
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

  add(worker, send) {
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
    ArrayPrototypePush(this.handles, handle);
    const [workerEntry] = this.free;

    if (ArrayIsArray(workerEntry)) {
      const [workerId, worker] = workerEntry;
      this.free.delete(workerId);
      this.handoff(worker);
    }
  }

  handoff(worker) {
    if (!this.all.has(worker.id)) {
      return;  // Worker is closing (or has closed) the server.
    }

    const handle = ArrayPrototypeShift(this.handles);

    if (handle === undefined) {
      this.free.set(worker.id, worker);  // Add to ready queue again.
      return;
    }

    const message = { act: 'newconn', key: this.key };

    sendHelper(worker.process, message, handle, (reply) => {
      if (reply.accepted)
        handle.close();
      else {
        // Worker is shutting down. Send to another.
        this.distribute(0, handle);
      }

      this.handoff(worker);
    });
  }
}

module.exports = RoundRobinHandle;
