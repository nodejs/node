'use strict';
const EventEmitter = require('events');
const internalUtil = require('internal/util');
const util = require('util');
const defineProperty = Object.defineProperty;
const suicideDeprecationMessage =
    'worker.suicide is deprecated. Please use worker.exitedAfterDisconnect.';

module.exports = Worker;

// Common Worker implementation shared between the cluster master and workers.
function Worker(options) {
  if (!(this instanceof Worker))
    return new Worker(options);

  EventEmitter.call(this);

  if (options === null || typeof options !== 'object')
    options = {};

  this.exitedAfterDisconnect = undefined;

  defineProperty(this, 'suicide', {
    get: internalUtil.deprecate(
      () => this.exitedAfterDisconnect,
      suicideDeprecationMessage, 'DEP0007'),
    set: internalUtil.deprecate(
      (val) => { this.exitedAfterDisconnect = val; },
      suicideDeprecationMessage, 'DEP0007'),
    enumerable: true
  });

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

util.inherits(Worker, EventEmitter);

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
