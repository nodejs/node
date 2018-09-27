'use strict';

const assert = require('assert');
const net = require('net');
const { sendHelper } = require('internal/cluster/utils');
const { internalBinding } = require('internal/bootstrap/loaders');
const uv = internalBinding('uv');

const relocate = Symbol('relocate');
const distribute = Symbol('distribute');
const handoff = Symbol('handoff');
const free = Symbol('free');

const kState = Symbol('state');
const kHash = Symbol('hash');
const kQueues = Symbol('queues');
const kWorkers = Symbol('workers');
const kHandle = Symbol('handle');
const kKey = Symbol('key');
const kServer = Symbol('server');

const STATE_PENDING = 0;
const STATE_CALLING = 1;

const hash = (ip) => {
  // Times33 string hashing function.
  // Support ipv4 and ipv6
  let hash = 5381;
  let i = ip.length;
  while (i) {
    hash = (hash * 33) ^ ip.charCodeAt(--i);
  }
  return hash >>> 0;
};

class IPHashHandle {
  constructor(key, address, port, addressType, fd) {
    this[kKey] = key;
    this[kWorkers] = [];
    this[kQueues] = [];
    this[kHandle] = null;
    this[kServer] = net.createServer(assert.fail);

    if (fd >= 0)
      this[kServer].listen({ fd });
    else if (port >= 0)
      this[kServer].listen(port, address);
    else
      assert.fail('not support UNIX domain socket');

    this[kServer].once('listening', () => {
      if (this[kWorkers].length === 0) { // All workers removed.
        return;
      }

      this[kHandle] = this[kServer]._handle;
      this[kHandle].onconnection = (err, handle) => {
        this[distribute](err, handle);
      };
      this[kServer]._handle = null;
      this[kServer] = null;
    });
  }

  add(worker, send) {
    assert(this[kWorkers].indexOf(worker) === -1);
    worker[kState] = STATE_PENDING;
    this[kWorkers].push(worker);
    this[relocate](); // Relocate handle.

    const done = () => {
      if (this[kWorkers].indexOf(worker) === -1) { // Already removed.
        return;
      }

      const out = {};
      const errno = this[kHandle].getsockname(out);
      if (errno === 0) {
        send(0, { sockname: out }, null);
      } else {
        send(errno, null);
        this.remove(worker);
        return;
      }

      this[handoff](worker);  // In case there are connections pending.
    };

    if (this[kServer] === null) {
      return done();
    }

    // Still busy binding.
    this[kServer].once('listening', done);
    this[kServer].once('error', (err) => {
      if (this[kWorkers].indexOf(worker) === -1) { // Already removed.
        return;
      }

      // Hack: translate 'EADDRINUSE' error string back to numeric error code.
      // It works but ideally we'd have some backchannel between the net and
      // cluster modules for stuff like this.
      send(uv[`UV_${err.errno}`], null);
    });
  }

  remove(worker) {
    const index = this[kWorkers].indexOf(worker);

    if (index >= 0) {
      this[kWorkers].splice(index, 1);
    } else {
      return false;
    }

    if (this[kWorkers].length > 0) {
      this[relocate](); // Relocate handle.
      return false;
    } else {
      this[free]();
      return true;
    }
  }

  [free]() {
    this[kQueues].forEach((queue) => queue.forEach((handle) => handle.close()));
    this[kQueues] = [];

    if (this[kHandle] !== null) { // Maybe still busy binding.
      this[kHandle].close();
      this[kHandle] = null;
    }
  }

  [relocate]() {
    const queues = [];
    for (var i = 0; i < this[kWorkers].length; i += 1) {
      queues.push([]);
    }

    for (const queue of this[kQueues]) {
      for (const handle of queue) {
        queues[handle[kHash] % this[kWorkers].length].push(handle);
      }
    }

    this[kQueues] = queues;
  }

  [distribute](err, handle) {
    if (err !== 0) {
      return;
    }

    const out = {};
    if (handle.getpeername(out) !== 0) {
      handle.close();
      return;
    }
    handle[kHash] = hash(out.address);

    const index = handle[kHash] % this[kWorkers].length;

    this[kQueues][index].push(handle);

    const worker = this[kWorkers][index];
    if (worker[kState] === STATE_PENDING) {
      this[handoff](worker);
    }
  }

  [handoff](worker) {
    const index = this[kWorkers].indexOf(worker);

    if (index === -1) {
      return; // Worker is closing (or has closed) the server.
    }

    const handle = this[kQueues][index].shift();

    if (handle === undefined) {
      worker[kState] = STATE_PENDING;
      return;
    } else {
      worker[kState] = STATE_CALLING;
    }

    const message = { act: 'newconn', key: this[kKey] };

    sendHelper(worker.process, message, handle, (reply) => {
      if (reply.accepted) {
        handle.close();
      } else {
        // Worker is shutting down, Add to queue again.
        this[kQueues][index].push(handle);
      }

      worker[kState] = STATE_PENDING;
      this[handoff](worker);
    });
  }
}

module.exports = IPHashHandle;
