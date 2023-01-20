'use strict';

const {
  ArrayPrototypeAt,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  Promise,
  PromisePrototypeThen,
  PromiseResolve,
  PromiseReject,
  ReflectApply,
  SafeMap,
  SymbolHasInstance,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const {
  validateFunction,
} = require('internal/validators');

const { triggerUncaughtException } = internalBinding('errors');

const { WeakReference } = internalBinding('util');

function decRef(channel) {
  if (channels.get(channel.name).decRef() === 0) {
    channels.delete(channel.name);
  }
}

function incRef(channel) {
  channels.get(channel.name).incRef();
}

function markActive(channel) {
  // eslint-disable-next-line no-use-before-define
  ObjectSetPrototypeOf(channel, ActiveChannel.prototype);
  channel._subscribers = [];
  channel._stores = new SafeMap();
}

function maybeMarkInactive(channel) {
  // When there are no more active subscribers or bound, restore to fast prototype.
  if (!channel._subscribers.length && !channel._stores.size) {
    // eslint-disable-next-line no-use-before-define
    ObjectSetPrototypeOf(channel, Channel.prototype);
    channel._subscribers = undefined;
    channel._stores = undefined;
  }
}

function defaultTransform(data) {
  return data;
}

function wrapStoreRun(store, data, next, transform = defaultTransform) {
  return () => {
    let context;
    try {
      context = transform(data);
    } catch (err) {
      process.nextTick(() => {
        triggerUncaughtException(err, false);
      });
      return next();
    }

    return store.run(context, next);
  };
}

// TODO(qard): should there be a C++ channel interface?
class ActiveChannel {
  subscribe(subscription) {
    validateFunction(subscription, 'subscription');
    ArrayPrototypePush(this._subscribers, subscription);
    incRef(this);
  }

  unsubscribe(subscription) {
    const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
    if (index === -1) return false;

    ArrayPrototypeSplice(this._subscribers, index, 1);

    decRef(this);
    maybeMarkInactive(this);

    return true;
  }

  bindStore(store, transform) {
    const replacing = this._stores.has(store);
    if (!replacing) incRef(this);
    this._stores.set(store, transform);
  }

  unbindStore(store) {
    if (!this._stores.has(store)) {
      return false;
    }

    this._stores.delete(store);

    decRef(this);
    maybeMarkInactive(this);

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

  runStores(data, fn, thisArg, ...args) {
    let run = () => {
      this.publish(data);
      return ReflectApply(fn, thisArg, args);
    };

    for (const entry of this._stores.entries()) {
      const store = entry[0];
      const transform = entry[1];
      run = wrapStoreRun(store, data, run, transform);
    }

    return run();
  }
}

class Channel {
  constructor(name) {
    this._subscribers = undefined;
    this._stores = undefined;
    this.name = name;

    channels.set(name, new WeakReference(this));
  }

  static [SymbolHasInstance](instance) {
    const prototype = ObjectGetPrototypeOf(instance);
    return prototype === Channel.prototype ||
           prototype === ActiveChannel.prototype;
  }

  subscribe(subscription) {
    markActive(this);
    this.subscribe(subscription);
  }

  unsubscribe() {
    return false;
  }

  bindStore(store, transform) {
    markActive(this);
    this.bindStore(store, transform);
  }

  unbindStore() {
    return false;
  }

  get hasSubscribers() {
    return false;
  }

  publish() {}

  runStores(data, fn, thisArg, ...args) {
    return ReflectApply(fn, thisArg, args);
  }
}

const channels = new SafeMap();

function channel(name) {
  let channel;
  const ref = channels.get(name);
  if (ref) channel = ref.get();
  if (channel) return channel;

  if (typeof name !== 'string' && typeof name !== 'symbol') {
    throw new ERR_INVALID_ARG_TYPE('channel', ['string', 'symbol'], name);
  }

  return new Channel(name);
}

function subscribe(name, subscription) {
  return channel(name).subscribe(subscription);
}

function unsubscribe(name, subscription) {
  return channel(name).unsubscribe(subscription);
}

function hasSubscribers(name) {
  let channel;
  const ref = channels.get(name);
  if (ref) channel = ref.get();
  if (!channel) {
    return false;
  }

  return channel.hasSubscribers;
}

const traceEvents = [
  'start',
  'end',
  'asyncStart',
  'asyncEnd',
  'error',
];

function assertChannel(value, name) {
  if (!(value instanceof Channel)) {
    throw new ERR_INVALID_ARG_TYPE(name, ['Channel'], value);
  }
}

class TracingChannel {
  constructor(nameOrChannels) {
    if (typeof nameOrChannels === 'string') {
      this.start = channel(`tracing:${nameOrChannels}:start`);
      this.end = channel(`tracing:${nameOrChannels}:end`);
      this.asyncStart = channel(`tracing:${nameOrChannels}:asyncStart`);
      this.asyncEnd = channel(`tracing:${nameOrChannels}:asyncEnd`);
      this.error = channel(`tracing:${nameOrChannels}:error`);
    } else if (typeof nameOrChannels === 'object') {
      const { start, end, asyncStart, asyncEnd, error } = nameOrChannels;

      assertChannel(start, 'nameOrChannels.start');
      assertChannel(end, 'nameOrChannels.end');
      assertChannel(asyncStart, 'nameOrChannels.asyncStart');
      assertChannel(asyncEnd, 'nameOrChannels.asyncEnd');
      assertChannel(error, 'nameOrChannels.error');

      this.start = start;
      this.end = end;
      this.asyncStart = asyncStart;
      this.asyncEnd = asyncEnd;
      this.error = error;
    } else {
      throw new ERR_INVALID_ARG_TYPE('nameOrChannels',
                                     ['string', 'object', 'Channel'],
                                     nameOrChannels);
    }
  }

  subscribe(handlers) {
    for (const name of traceEvents) {
      if (!handlers[name]) continue;

      this[name]?.subscribe(handlers[name]);
    }
  }

  unsubscribe(handlers) {
    let done = true;

    for (const name of traceEvents) {
      if (!handlers[name]) continue;

      if (!this[name]?.unsubscribe(handlers[name])) {
        done = false;
      }
    }

    return done;
  }

  traceSync(fn, ctx = {}, thisArg, ...args) {
    const { start, end, error } = this;

    try {
      const result = start.runStores(ctx, fn, thisArg, ...args);
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
    const { start, end, asyncStart, asyncEnd, error } = this;

    function reject(err) {
      ctx.error = err;
      error.publish(ctx);
      asyncStart.publish(ctx);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      asyncEnd.publish(ctx);
      return PromiseReject(err);
    }

    function resolve(result) {
      ctx.result = result;
      asyncStart.publish(ctx);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      asyncEnd.publish(ctx);
      return result;
    }

    try {
      let promise = start.runStores(ctx, fn, thisArg, ...args);
      // Convert thenables to native promises
      if (!(promise instanceof Promise)) {
        promise = PromiseResolve(promise);
      }
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
    const { start, end, asyncStart, asyncEnd, error } = this;

    function wrappedCallback(err, res) {
      if (err) {
        ctx.error = err;
        error.publish(ctx);
      } else {
        ctx.result = res;
      }

      asyncStart.publish(ctx);
      try {
        if (callback) {
          return ReflectApply(callback, this, arguments);
        }
      } finally {
        asyncEnd.publish(ctx);
      }
    }

    const callback = ArrayPrototypeAt(args, position);
    if (typeof callback !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('callback', ['function'], callback);
    }
    ArrayPrototypeSplice(args, position, 1, wrappedCallback);

    try {
      return start.runStores(ctx, fn, thisArg, ...args);
    } catch (err) {
      ctx.error = err;
      error.publish(ctx);
      throw err;
    } finally {
      end.publish(ctx);
    }
  }
}

function tracingChannel(nameOrChannels) {
  return new TracingChannel(nameOrChannels);
}

module.exports = {
  channel,
  hasSubscribers,
  subscribe,
  tracingChannel,
  unsubscribe,
  Channel,
};
