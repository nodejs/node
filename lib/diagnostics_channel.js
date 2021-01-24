'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  ObjectCreate,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  SymbolHasInstance,
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
  if (ref) channel = ref.get();
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
