'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ArrayPrototypeSplice,
  ObjectCreate,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  Promise,
  PromiseResolve,
  SymbolAsyncIterator,
  SymbolHasInstance,
  WeakRefPrototypeGet
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  }
} = require('internal/errors');

const { triggerUncaughtException } = internalBinding('errors');

const { WeakReference } = internalBinding('util');

class AsyncIterableChannel {
  constructor(channel) {
    this.channel = channel;
    this.events = [];
    this.waiting = [];

    this.subscriber = (message) => {
      const resolve = ArrayPrototypeShift(this.waiting);
      if (resolve) {
        return resolve({
          value: message,
          done: false
        });
      }

      ArrayPrototypePush(this.events, message);
    };

    channel.subscribe(this.subscriber);
  }

  [SymbolAsyncIterator]() {
    return this;
  }

  return() {
    const data = { done: true };
    this.done = true;

    this.channel.unsubscribe(this.subscriber);

    for (let i = 0; i < this.waiting.length; i++) {
      const resolve = this.waiting[i];
      resolve(data);
    }

    return PromiseResolve(data);
  }

  next() {
    const event = ArrayPrototypeShift(this.events);
    if (event) {
      return PromiseResolve({
        value: event,
        done: false
      });
    }

    if (this.done) {
      return PromiseResolve({
        done: true
      });
    }

    return new Promise((resolve) => {
      ArrayPrototypePush(this.waiting, resolve);
    });
  }
}

// TODO(qard): should there be a C++ channel interface?
class ActiveChannel {
  [SymbolAsyncIterator]() {
    return new AsyncIterableChannel(this);
  }

  subscribe(subscription) {
    if (typeof subscription !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('subscription', ['function'],
                                     subscription);
    }
    ArrayPrototypePush(this._subscribers, subscription);
  }

  unsubscribe(subscription) {
    const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
    if (index >= 0) {
      ArrayPrototypeSplice(this._subscribers, index, 1);

      // When there are no more active subscribers, restore to fast prototype.
      if (!this._subscribers.length) {
        // eslint-disable-next-line no-use-before-define
        ObjectSetPrototypeOf(this, Channel.prototype);
      }
    }
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

  [SymbolAsyncIterator]() {
    return new AsyncIterableChannel(this);
  }

  subscribe(subscription) {
    ObjectSetPrototypeOf(this, ActiveChannel.prototype);
    this._subscribers = [];
    this.subscribe(subscription);
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

function hasSubscribers(name) {
  let channel;
  const ref = channels[name];
  if (ref) channel = WeakRefPrototypeGet(ref);
  if (!channel) {
    return false;
  }

  return channel.hasSubscribers;
}

module.exports = {
  channel,
  hasSubscribers,
  Channel
};
