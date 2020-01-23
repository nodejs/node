'use strict';

const {
  ObjectSetPrototypeOf,
} = primordials;

const EventEmitter = require('events');

module.exports = Worker;

// Common Worker implementation shared between the cluster master and workers.
function Worker(options) {
  if (!(this instanceof Worker))
    return new Worker(options);

  EventEmitter.call(this);

  if (options === null || typeof options !== 'object')
    options = {};

  this.exitedAfterDisconnect = undefined;

  this.state = options.state || 'none';
  this.id = options.id | 0;

  if (options.process) {
    this.process = options.process;
    this.process.on('error', (code, signal) =>
      this.emit('error', code, signal)
    );
    this.process.on('message', (message, handle) =>
      this.emit('message', message, handle)
    );
  }
}

ObjectSetPrototypeOf(Worker.prototype, EventEmitter.prototype);
ObjectSetPrototypeOf(Worker, EventEmitter);

Worker.prototype.kill = function() {
  this.destroy.apply(this, arguments);
};

Worker.prototype.send = function() {
  return this.process.send.apply(this.process, arguments);
};

Worker.prototype.isDead = function() {
  return this.process.exitCode != null || this.process.signalCode != null;
};

Worker.prototype.isConnected = function() {
  return this.process.connected;
};
