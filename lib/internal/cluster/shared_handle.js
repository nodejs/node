'use strict';
const { Map } = primordials;
const assert = require('internal/assert');
const dgram = require('internal/dgram');
const net = require('net');

class SharedHandle {
  constructor(key, address, { port, addressType, fd, flags }) {
    this.key = key;
    this.workers = new Map();
    this.handle = null;
    this.errno = 0;
    const rval = this.determineRval(addressType,
                                    address,
                                    port,
                                    fd,
                                    flags);
    if (typeof rval === 'number')
      this.errno = rval;
    else
      this.handle = rval;
  }

  determineRval(addressType, address, port, fd, flags) {
    if (addressType === 'udp4' || addressType === 'udp6')
      return dgram._createSocketHandle(address, port, addressType, fd, flags);
    else
      return net._createServerHandle(address, port, addressType, fd, flags);
  }

  add(worker, send) {
    assert(!this.workers.has(worker.id));
    this.workers.set(worker.id, worker);
    send(this.errno, null, this.handle);
  }

  remove(worker) {
    if (!this.workers.has(worker.id))
      return false; // The worker wasn't sharing this handle.
    this.workers.delete(worker.id);
    if (this.workers.size !== 0)
      return false;
    this.handle.close();
    this.handle = null;
    return true;
  }
}

module.exports = SharedHandle;
