'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSome,
  ArrayPrototypeSplice,
  ObjectCreate,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  ReflectApply,
  SymbolHasInstance
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  }
} = require('internal/errors');
const {
  validateFunction,
} = require('internal/validators');

const { triggerUncaughtException } = internalBinding('errors');

const { WeakReference } = internalBinding('util');

// TODO(qard): should there be a C++ channel interface?
class ActiveChannel {
  subscribe(subscription) {
    validateFunction(subscription, 'subscription');
    ArrayPrototypePush(this._subscribers, subscription);
  }

  unsubscribe(subscription) {
    const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
    if (index === -1) return false;

    ArrayPrototypeSplice(this._subscribers, index, 1);

    // When there are no more active subscribers, restore to fast prototype.
    if (!this._subscribers.length) {
      // eslint-disable-next-line no-use-before-define
      ObjectSetPrototypeOf(this, Channel.prototype);
    }

    return true;
  }

  get hasSubscribers() {
    return true;
  }

  publish(data) {
    for (let i = 0; i < this._subscribers.length; i++) {
      try {
        const onMessage = this._subscribers[i];
        onMessage(data, this.name);
      } catch (err) {
        process.nextTick(() => {
          triggerUncaughtException(err, false);
        });
      }
    }
  }
}

class Channel {
  constructor(name) {
    this._subscribers = undefined;
    this.name = name;
  }

  static [SymbolHasInstance](instance) {
    const prototype = ObjectGetPrototypeOf(instance);
    return prototype === Channel.prototype ||
           prototype === ActiveChannel.prototype;
  }

  subscribe(subscription) {
    ObjectSetPrototypeOf(this, ActiveChannel.prototype);
    this._subscribers = [];
    this.subscribe(subscription);
  }

  unsubscribe() {
    return false;
  }

  get hasSubscribers() {
    return false;
  }

  publish() {}
}

const channels = ObjectCreate(null);

function channel(name) {
  let channel;
  const ref = channels[name];
  if (ref) channel = ref.get();
  if (channel) return channel;

  if (typeof name !== 'string' && typeof name !== 'symbol') {
    throw new ERR_INVALID_ARG_TYPE('channel', ['string', 'symbol'], name);
  }

  channel = new Channel(name);
  channels[name] = new WeakReference(channel);
  return channel;
}

function subscribe(name, subscription) {
  const chan = channel(name);
  channels[name].incRef();
  chan.subscribe(subscription);
}

function unsubscribe(name, subscription) {
  const chan = channel(name);
  if (!chan.unsubscribe(subscription)) {
    return false;
  }

  channels[name].decRef();
  return true;
}

function hasSubscribers(name) {
  let channel;
  const ref = channels[name];
  if (ref) channel = ref.get();
  if (!channel) {
    return false;
  }

  return channel.hasSubscribers;
}

class StoreBinding {
  constructor(store, build = (v) => v) {
    this.store = store;
    this.build = build;
    this.stack = [];

    this.onEnter = this.onEnter.bind(this);
    this.onExit = this.onExit.bind(this);
  }

  onEnter(data) {
    try {
      const ctx = this.build(data);
      this.stack.push(this.store.getStore());
      this.store.enterWith(ctx);
    } catch (err) {
      process.nextTick(() => {
        triggerUncaughtException(err, false);
      });
    }
  }

  onExit() {
    if (!this.stack.length) return;
    this.store.enterWith(this.stack.pop());
  }
}

class ActiveStorageChannel {
  get hasSubscribers() {
    return true;
  }

  isBoundToStore(store) {
    return ArrayPrototypeSome(this._bindings, (v) => v.store === store);
  }

  _bindStore(store, build) {
    if (this.isBoundToStore(store)) return false;
    ArrayPrototypePush(this._bindings, new StoreBinding(store, build));
    return true;
  }

  _unbindStore(store) {
    if (!this.isBoundToStore(store)) return false;

    let found = false;
    for (let index = 0; index < this._bindings.length; index++) {
      if (this._bindings[index].store === store) {
        ArrayPrototypeSplice(this._bindings, index, 1);
        found = true;
        break;
      }
    }

    if (!this._bindings.length) {
      // eslint-disable-next-line no-use-before-define
      ObjectSetPrototypeOf(this, StorageChannel.prototype);
      this._bindings = undefined;
    }

    return found;
  }

  _enter(data) {
    for (const binding of this._bindings) {
      binding.onEnter(data);
    }
  }

  _exit() {
    for (const binding of this._bindings) {
      binding.onExit();
    }
  }

  run(data, fn, thisArg, ...args) {
    this._enter(data);
    try {
      return ReflectApply(fn, thisArg, args);
    } finally {
      this._exit();
    }
  }
}

class StorageChannel {
  constructor() {
    this._bindings = undefined;
  }

  static [SymbolHasInstance](instance) {
    const prototype = ObjectGetPrototypeOf(instance);
    return prototype === StorageChannel.prototype ||
      prototype === ActiveStorageChannel.prototype;
  }

  get hasSubscribers() {
    return false;
  }

  isBoundToStore(_) {
    return false;
  }

  _bindStore(store, build) {
    ObjectSetPrototypeOf(this, ActiveStorageChannel.prototype);
    this._bindings = [];
    return this._bindStore(store, build);
  }

  _unbindStore(_) {
    return false;
  }

  _enter(_) {}
  _exit() {}

  run(_, fn, thisArg, ...args) {
    return ReflectApply(fn, thisArg, args);
  }
}

const storageChannels = ObjectCreate(null);

function storageChannel(name) {
  let channel;
  const ref = storageChannels[name];
  if (ref) channel = ref.get();
  if (channel) return channel;

  if (typeof name !== 'string' && typeof name !== 'symbol') {
    throw new ERR_INVALID_ARG_TYPE('channel', ['string', 'symbol'], name);
  }

  channel = new StorageChannel(name);
  storageChannels[name] = new WeakReference(channel);
  return channel;
}

function bindStore(name, store, build) {
  const chan = storageChannel(name);
  storageChannels[name].incRef();
  chan._bindStore(store, build);
}

function unbindStore(name, store) {
  const chan = storageChannel(name);
  if (!chan._unbindStore(store)) {
    return false;
  }

  storageChannels[name].decRef();
  return true;
}

module.exports = {
  bindStore,
  channel,
  hasSubscribers,
  storageChannel,
  subscribe,
  unbindStore,
  unsubscribe,
  Channel
};
