'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  ObjectCreate,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  ReflectApply,
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
    this._weak.incRef();
  }

  unsubscribe(subscription) {
    const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
    if (index === -1) return false;

    ArrayPrototypeSplice(this._subscribers, index, 1);
    this._weak.decRef();

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
    this._weak = undefined;
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
  channel._weak = new WeakReference(channel);
  channels[name] = channel._weak;
  return channel;
}

function subscribe(name, subscription) {
  return channel(name).subscribe(subscription);
}

function unsubscribe(name, subscription) {
  return channel(name).unsubscribe(subscription);
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

class TracingChannel {
  #channels;

  constructor(name) {
    this.name = name;
    this.#channels = {
      start: new Channel(Symbol(`tracing.start`)),
      end: new Channel(Symbol(`tracing.end`)),
      asyncEnd: new Channel(Symbol(`tracing.asyncEnd`)),
      error: new Channel(Symbol(`tracing.error`))
    };
  }

  // Attach WeakReference to all the sub-channels so the liveness management
  // in subscribe/unsubscribe keeps the TracingChannel the sub-channels are
  // attached to alive.
  set _weak(weak) {
    for (const key in this.#channels) {
      this.#channels[key]._weak = weak;
    }
  }

  get hasSubscribers() {
    for (const key in this.#channels) {
      if (this.#channels[key].hasSubscribers) {
        return true;
      }
    }
    return false;
  }

  subscribe(handlers) {
    for (const key in handlers) {
      this.#channels[key]?.subscribe(handlers[key]);
    }
  }

  unsubscribe(handlers) {
    for (const key in handlers) {
      const channel = this.#channels[key];
      if (!channel || !channel.unsubscribe(handlers[key])) {
        return false;
      }
    }
    return true;
  }

  traceSync(fn, ctx = {}, thisArg, ...args) {
    if (!this.hasSubscribers) return ReflectApply(fn, thisArg, args);
    const { start, end, error } = this.#channels;
    start.publish(ctx);
    try {
      const result = ReflectApply(fn, thisArg, args);
      ctx.result = result;
      return result;
    } catch (err) {
      ctx.error = err;
      error.publish(ctx);
      throw err;
    } finally {
      end.publish(ctx);
    }
  }

  tracePromise(fn, ctx = {}, thisArg, ...args) {
    if (!this.hasSubscribers) return ReflectApply(fn, thisArg, args);
    const { asyncEnd, start, end, error } = this.#channels;
    start.publish(ctx);

    const reject = (err) => {
      ctx.error = err;
      error.publish(ctx);
      asyncEnd.publish(ctx);
      throw err;
    };

    const resolve = (result) => {
      ctx.result = result;
      asyncEnd.publish(ctx);
      return result;
    };

    try {
      const promise = ReflectApply(fn, thisArg, args);
      return PromisePrototypeThen(promise, resolve, reject);
    } catch (err) {
      ctx.error = err;
      error.publish(ctx);
      throw err;
    } finally {
      end.publish(ctx);
    }
  }

  traceCallback(fn, position = 0, ctx = {}, thisArg, ...args) {
    if (!this.hasSubscribers) return ReflectApply(fn, thisArg, args);
    const { start, end, asyncEnd, error } = this.#channels;
    start.publish(ctx);

    function wrap(fn) {
      return function wrappedCallback (err, res) {
        if (err) {
          ctx.error = err;
          error.publish(ctx);
        } else {
          ctx.result = res;
        }

        asyncEnd.publish(ctx);
        if (fn) {
          return ReflectApply(fn, this, arguments);
        }
      }
    }

    if (position >= 0) {
      args.splice(position, 1, wrap(args.at(position)));
    }

    try {
      return ReflectApply(fn, thisArg, args);
    } catch (err) {
      ctx.error = err;
      error.publish(ctx);
      throw err;
    } finally {
      end.publish(ctx);
    }
  }
}

const tracingChannels = ObjectCreate(null);

function tracingChannel(name) {
  let channel;
  const ref = tracingChannels[name];
  if (ref) channel = ref.get();
  if (channel) return channel;

  if (typeof name !== 'string' && typeof name !== 'symbol') {
    throw new ERR_INVALID_ARG_TYPE('tracingChannel', ['string', 'symbol'], name);
  }

  channel = new TracingChannel(name);
  channel._weak = new WeakReference(channel);
  tracingChannels[name] = channel._weak;
  return channel;
}

module.exports = {
  channel,
  hasSubscribers,
  subscribe,
  tracingChannel,
  unsubscribe,
  Channel,
  TracingChannel
};
