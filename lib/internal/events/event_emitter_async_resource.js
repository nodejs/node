'use strict';

const {
  ArrayPrototypeUnshift,
  ReflectApply,
  Symbol,
} = primordials;
const {
  codes: {
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const { validateString } = require('internal/validators');

const {
  AsyncResource,
} = require('async_hooks');
const EventEmitter = require('events');

const kEventEmitter = Symbol('kEventEmitter');
const kAsyncResource = Symbol('kAsyncResource');
class EventEmitterReferencingAsyncResource extends AsyncResource {
  /**
   * @param {EventEmitter} ee
   * @param {string} [type]
   * @param {{
   *   triggerAsyncId?: number,
   *   requireManualDestroy?: boolean,
   * }} [options]
   */
  constructor(ee, type, options) {
    super(type, options);
    this[kEventEmitter] = ee;
  }

  /**
   * @type {EventEmitter}
   */
  get eventEmitter() {
    if (this[kEventEmitter] === undefined)
      throw new ERR_INVALID_THIS('EventEmitterReferencingAsyncResource');
    return this[kEventEmitter];
  }
}

class EventEmitterAsyncResource extends EventEmitter {
  /**
   * @param {{
   *   name?: string,
   *   triggerAsyncId?: number,
   *   requireManualDestroy?: boolean,
   * }} [options]
   */
  constructor(options = undefined) {
    let name;
    if (typeof options === 'string') {
      name = options;
      options = undefined;
    } else {
      if (new.target === EventEmitterAsyncResource) {
        validateString(options?.name, 'options.name');
      }
      name = options?.name || new.target.name;
    }
    super(options);

    this[kAsyncResource] =
      new EventEmitterReferencingAsyncResource(this, name, options);
  }

  /**
   * @param {symbol,string} event
   * @param  {...any} args
   * @returns {boolean}
   */
  emit(event, ...args) {
    if (this[kAsyncResource] === undefined)
      throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
    const { asyncResource } = this;
    ArrayPrototypeUnshift(args, super.emit, this, event);
    return ReflectApply(asyncResource.runInAsyncScope, asyncResource,
                        args);
  }

  /**
   * @returns {void}
   */
  emitDestroy() {
    if (this[kAsyncResource] === undefined)
      throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
    this.asyncResource.emitDestroy();
  }

  /**
   * @type {number}
   */
  get asyncId() {
    if (this[kAsyncResource] === undefined)
      throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
    return this.asyncResource.asyncId();
  }

  /**
   * @type {number}
   */
  get triggerAsyncId() {
    if (this[kAsyncResource] === undefined)
      throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
    return this.asyncResource.triggerAsyncId();
  }

  /**
   * @type {EventEmitterReferencingAsyncResource}
   */
  get asyncResource() {
    if (this[kAsyncResource] === undefined)
      throw new ERR_INVALID_THIS('EventEmitterAsyncResource');
    return this[kAsyncResource];
  }
}

module.exports = EventEmitterAsyncResource;
