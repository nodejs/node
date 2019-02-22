'use strict';
const assert = require('internal/assert');
const dgram = require('internal/dgram');
const net = require('net');

module.exports = SharedHandle;

function SharedHandle(key, address, port, addressType, fd, flags) {
  this.key = key;
  this.workers = [];
  this.handle = null;
  this.errno = 0;

  var rval;
  if (addressType === 'udp4' || addressType === 'udp6')
    rval = dgram._createSocketHandle(address, port, addressType, fd, flags);
  else
    rval = net._createServerHandle(address, port, addressType, fd, flags);

  if (typeof rval === 'number')
    this.errno = rval;
  else
    this.handle = rval;
}

SharedHandle.prototype.add = function(worker, send) {
  assert(this.workers.indexOf(worker) === -1);
  this.workers.push(worker);
  send(this.errno, null, this.handle);
};

SharedHandle.prototype.remove = function(worker) {
  const index = this.workers.indexOf(worker);

  if (index === -1)
    return false; // The worker wasn't sharing this handle.

  this.workers.splice(index, 1);

  if (this.workers.length !== 0)
    return false;

  this.handle.close();
  this.handle = null;
  return true;
};
