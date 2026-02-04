'use strict';

const {
  ArrayPrototypeAt,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  ReflectApply,
  SafeFinalizationRegistry,
  SafeMap,
  SymbolDispose,
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

const { WeakReference } = require('internal/util');

// Can't delete when weakref count reaches 0 as it could increment again.
// Only GC can be used as a valid time to clean up the channels map.
class WeakRefMap extends SafeMap {
  #finalizers = new SafeFinalizationRegistry((key) => {
    // Check that the key doesn't have any value before deleting, as the WeakRef for the key
    // may have been replaced since finalization callbacks aren't synchronous with GC.
    if (!this.has(key)) this.delete(key);
  });

  set(key, value) {
    this.#finalizers.register(value, key);
    return super.set(key, new WeakReference(value));
  }

  get(key) {
    return super.get(key)?.get();
  }

  has(key) {
    return !!this.get(key);
  }

  incRef(key) {
    return super.get(key)?.incRef();
  }

  decRef(key) {
    return super.get(key)?.decRef();
  }
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

class RunStoresScope {
  #stack;

  constructor(activeChannel, data) {
    // eslint-disable-next-line no-restricted-globals
    using stack = new DisposableStack();

    // Enter stores using withScope
    if (activeChannel._stores) {
      for (const entry of activeChannel._stores.entries()) {
        const store = entry[0];
        const transform = entry[1];

        let newContext = data;
        if (transform) {
          try {
            newContext = transform(data);
          } catch (err) {
            process.nextTick(() => {
              triggerUncaughtException(err, false);
            });
            continue;
          }
        }

        stack.use(store.withScope(newContext));
      }
    }

    // Publish data
    activeChannel.publish(data);

    // Transfer ownership of the stack
    this.#stack = stack.move();
  }

  [SymbolDispose]() {
    this.#stack[SymbolDispose]();
  }
}

// TODO(qard): should there be a C++ channel interface?
class ActiveChannel {
  subscribe(subscription) {
    validateFunction(subscription, 'subscription');
    this._subscribers = ArrayPrototypeSlice(this._subscribers);
    ArrayPrototypePush(this._subscribers, subscription);
    channels.incRef(this.name);
  }

  unsubscribe(subscription) {
    const index = ArrayPrototypeIndexOf(this._subscribers, subscription);
    if (index === -1) return false;

    const before = ArrayPrototypeSlice(this._subscribers, 0, index);
    const after = ArrayPrototypeSlice(this._subscribers, index + 1);
    this._subscribers = before;
    ArrayPrototypePushApply(this._subscribers, after);

    channels.decRef(this.name);
    maybeMarkInactive(this);

    return true;
  }

  bindStore(store, transform) {
    const replacing = this._stores.has(store);
    if (!replacing) channels.incRef(this.name);
    this._stores.set(store, transform);
  }

  unbindStore(store) {
    if (!this._stores.has(store)) {
      return false;
    }

    this._stores.delete(store);

    channels.decRef(this.name);
    maybeMarkInactive(this);

    return true;
  }

  get hasSubscribers() {
    return true;
  }

  publish(data) {
    const subscribers = this._subscribers;
    for (let i = 0; i < (subscribers?.length || 0); i++) {
      try {
        const onMessage = subscribers[i];
        onMessage(data, this.name);
      } catch (err) {
        process.nextTick(() => {
          triggerUncaughtException(err, false);
        });
      }
    }
  }

  withStoreScope(data) {
    return new RunStoresScope(this, data);
  }

  runStores(data, fn, thisArg, ...args) {
    // eslint-disable-next-line no-unused-vars
    using scope = this.withStoreScope(data);
    return ReflectApply(fn, thisArg, args);
  }
}

class Channel {
  constructor(name) {
    this._subscribers = undefined;
    this._stores = undefined;
    this.name = name;

    channels.set(name, this);
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

  withStoreScope() {
    // Return no-op disposable for inactive channels
    return {
      [SymbolDispose]() {},
    };
  }
}

const channels = new WeakRefMap();

function channel(name) {
  const channel = channels.get(name);
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
  const channel = channels.get(name);
  if (!channel) return false;

  return channel.hasSubscribers;
}

const windowEvents = [
  'start',
  'end',
];

function assertChannel(value, name) {
  if (!(value instanceof Channel)) {
    throw new ERR_INVALID_ARG_TYPE(name, ['Channel'], value);
  }
}

function channelFromMap(nameOrChannels, name, className) {
  if (typeof nameOrChannels === 'string') {
    return channel(`tracing:${nameOrChannels}:${name}`);
  }

  if (typeof nameOrChannels === 'object' && nameOrChannels !== null) {
    const channel = nameOrChannels[name];
    assertChannel(channel, `nameOrChannels.${name}`);
    return channel;
  }

  throw new ERR_INVALID_ARG_TYPE('nameOrChannels',
                                 ['string', 'object', className],
                                 nameOrChannels);
}

class WindowChannelScope {
  #context;
  #end;
  #scope;

  constructor(windowChannel, context) {
    // Only proceed if there are subscribers
    if (!windowChannel.hasSubscribers) {
      return;
    }

    const { start, end } = windowChannel;
    this.#context = context;
    this.#end = end;

    // Use RunStoresScope for the start channel
    this.#scope = new RunStoresScope(start, context);
  }

  [SymbolDispose]() {
    if (!this.#scope) {
      return;
    }

    // Publish end event
    this.#end.publish(this.#context);

    // Dispose the start scope to restore stores
    this.#scope[SymbolDispose]();
    this.#scope = undefined;
  }
}

class WindowChannel {
  constructor(nameOrChannels) {
    for (let i = 0; i < windowEvents.length; ++i) {
      const eventName = windowEvents[i];
      ObjectDefineProperty(this, eventName, {
        __proto__: null,
        value: channelFromMap(nameOrChannels, eventName, 'WindowChannel'),
      });
    }
  }

  get hasSubscribers() {
    return this.start?.hasSubscribers ||
      this.end?.hasSubscribers;
  }

  subscribe(handlers) {
    for (let i = 0; i < windowEvents.length; ++i) {
      const name = windowEvents[i];
      if (!handlers[name]) continue;

      this[name]?.subscribe(handlers[name]);
    }
  }

  unsubscribe(handlers) {
    let done = true;

    for (let i = 0; i < windowEvents.length; ++i) {
      const name = windowEvents[i];
      if (!handlers[name]) continue;

      if (!this[name]?.unsubscribe(handlers[name])) {
        done = false;
      }
    }

    return done;
  }

  withScope(context = {}) {
    return new WindowChannelScope(this, context);
  }

  run(context, fn, thisArg, ...args) {
    context ??= {};
    // eslint-disable-next-line no-unused-vars
    using scope = this.withScope(context);
    return ReflectApply(fn, thisArg, args);
  }
}

function windowChannel(nameOrChannels) {
  return new WindowChannel(nameOrChannels);
}

class TracingChannel {
  #callWindow;
  #continuationWindow;

  constructor(nameOrChannels) {
    // Create a WindowChannel for start/end (call window)
    if (typeof nameOrChannels === 'string') {
      this.#callWindow = new WindowChannel(nameOrChannels);
      this.#continuationWindow = new WindowChannel({
        start: channel(`tracing:${nameOrChannels}:asyncStart`),
        end: channel(`tracing:${nameOrChannels}:asyncEnd`),
      });
    } else if (typeof nameOrChannels === 'object') {
      this.#callWindow = new WindowChannel({
        start: nameOrChannels.start,
        end: nameOrChannels.end,
      });
      this.#continuationWindow = new WindowChannel({
        start: nameOrChannels.asyncStart,
        end: nameOrChannels.asyncEnd,
      });
    }

    // Create individual channel for error
    ObjectDefineProperty(this, 'error', {
      __proto__: null,
      value: channelFromMap(nameOrChannels, 'error', 'TracingChannel'),
    });
  }

  get start() {
    return this.#callWindow.start;
  }

  get end() {
    return this.#callWindow.end;
  }

  get asyncStart() {
    return this.#continuationWindow.start;
  }

  get asyncEnd() {
    return this.#continuationWindow.end;
  }

  get hasSubscribers() {
    return this.#callWindow.hasSubscribers ||
      this.#continuationWindow.hasSubscribers ||
      this.error?.hasSubscribers;
  }

  subscribe(handlers) {
    // Subscribe to call window (start/end)
    if (handlers.start || handlers.end) {
      this.#callWindow.subscribe({
        start: handlers.start,
        end: handlers.end,
      });
    }

    // Subscribe to continuation window (asyncStart/asyncEnd)
    if (handlers.asyncStart || handlers.asyncEnd) {
      this.#continuationWindow.subscribe({
        start: handlers.asyncStart,
        end: handlers.asyncEnd,
      });
    }

    // Subscribe to error channel
    if (handlers.error) {
      this.error.subscribe(handlers.error);
    }
  }

  unsubscribe(handlers) {
    let done = true;

    // Unsubscribe from call window
    if (handlers.start || handlers.end) {
      if (!this.#callWindow.unsubscribe({
        start: handlers.start,
        end: handlers.end,
      })) {
        done = false;
      }
    }

    // Unsubscribe from continuation window
    if (handlers.asyncStart || handlers.asyncEnd) {
      if (!this.#continuationWindow.unsubscribe({
        start: handlers.asyncStart,
        end: handlers.asyncEnd,
      })) {
        done = false;
      }
    }

    // Unsubscribe from error channel
    if (handlers.error) {
      if (!this.error.unsubscribe(handlers.error)) {
        done = false;
      }
    }

    return done;
  }

  traceSync(fn, context = {}, thisArg, ...args) {
    if (!this.hasSubscribers) {
      return ReflectApply(fn, thisArg, args);
    }

    const { error } = this;

    // eslint-disable-next-line no-unused-vars
    using scope = this.#callWindow.withScope(context);
    try {
      const result = ReflectApply(fn, thisArg, args);
      context.result = result;
      return result;
    } catch (err) {
      context.error = err;
      error.publish(context);
      throw err;
    }
  }

  tracePromise(fn, context = {}, thisArg, ...args) {
    if (!this.hasSubscribers) {
      return ReflectApply(fn, thisArg, args);
    }

    const { error } = this;
    const continuationWindow = this.#continuationWindow;

    function reject(err) {
      context.error = err;
      error.publish(context);
      // Use continuation window for asyncStart/asyncEnd
      // eslint-disable-next-line no-unused-vars
      using scope = continuationWindow.withScope(context);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      return PromiseReject(err);
    }

    function resolve(result) {
      context.result = result;
      // Use continuation window for asyncStart/asyncEnd
      // eslint-disable-next-line no-unused-vars
      using scope = continuationWindow.withScope(context);
      // TODO: Is there a way to have asyncEnd _after_ the continuation?
      return result;
    }

    // eslint-disable-next-line no-unused-vars
    using scope = this.#callWindow.withScope(context);
    try {
      let promise = ReflectApply(fn, thisArg, args);
      // Convert thenables to native promises
      if (!(promise instanceof Promise)) {
        promise = PromiseResolve(promise);
      }
      return PromisePrototypeThen(promise, resolve, reject);
    } catch (err) {
      context.error = err;
      error.publish(context);
      throw err;
    }
  }

  traceCallback(fn, position = -1, context = {}, thisArg, ...args) {
    if (!this.hasSubscribers) {
      return ReflectApply(fn, thisArg, args);
    }

    const { error } = this;
    const continuationWindow = this.#continuationWindow;

    function wrappedCallback(err, res) {
      if (err) {
        context.error = err;
        error.publish(context);
      } else {
        context.result = res;
      }

      // Use continuation window for asyncStart/asyncEnd around callback
      // eslint-disable-next-line no-unused-vars
      using scope = continuationWindow.withScope(context);
      return ReflectApply(callback, this, arguments);
    }

    const callback = ArrayPrototypeAt(args, position);
    validateFunction(callback, 'callback');
    ArrayPrototypeSplice(args, position, 1, wrappedCallback);

    // eslint-disable-next-line no-unused-vars
    using scope = this.#callWindow.withScope(context);
    try {
      return ReflectApply(fn, thisArg, args);
    } catch (err) {
      context.error = err;
      error.publish(context);
      throw err;
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
  windowChannel,
  Channel,
  WindowChannel,
};
