'use strict';
const assert = require('assert');
const net = require('net');
const { sendHelper, hash } = require('internal/cluster/utils');
const getOwnPropertyNames = Object.getOwnPropertyNames;
const uv = process.binding('uv');

module.exports = StickyHandle;

function StickyHandle(key, address, port, addressType, fd) {
  this.key = key;
  this.all = {};
  this.workers = [];
  this.handles = [];
  this.handle = null;
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

StickyHandle.prototype.add = function(worker, send) {
  assert(worker.id in this.all === false);
  this.all[worker.id] = worker;
  this.workers.push(worker);

  const done = () => {
    if (this.handle.getsockname) {
      const out = {};
      this.handle.getsockname(out);
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

StickyHandle.prototype.remove = function(worker) {
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

StickyHandle.prototype.distribute = function(err, handle) {
  this.handles.push(handle);

  const out = {};
  handle.getpeername(out);
  var index = hash(out.address || '') % this.workers.length;

  const worker = this.workers[index];

  if (worker)
    this.handoff(worker);
};

StickyHandle.prototype.handoff = function(worker) {
  if (worker.id in this.all === false) {
    return;  // Worker is closing (or has closed) the server.
  }

  const handle = this.handles.shift();

  if (handle === undefined) {
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
