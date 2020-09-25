'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  Map,
  MapPrototypeDelete,
  MapPrototypeGet,
  MapPrototypeSet,
  ObjectCreate,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
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

function identity(v) {
  return v;
}

class SpanMessage {
  constructor(action, id, data) {
    this.action = action;
    this.id = id;
    this.data = data;
  }

  // Augment with type: span to identify over the wire through JSON
  toJSON() {
    return {
      ...this,
      type: 'span'
    };
  }
}

let spanId = 0;

class InactiveSpan {
  annotate() {}
  end() {}
}

class Span {
  constructor(channel, data) {
    this._channel = channel;
    this.id = ++spanId;
    this._ended = false;

    this._message('start', data);
  }

  _message(action, data) {
    if (this._ended) return;

    if (action === 'end') {
      this._ended = true;
    }

    this._channel.publish(new SpanMessage(action, this.id, data));
  }

  annotate(data) {
    this._message('annotation', data);
  }

  end(data) {
    this._message('end', data);
  }

  static aggregate(channel, map = identity, onComplete) {
    const spans = new Map();

    if (typeof onComplete !== 'function') {
      onComplete = map;
      map = identity;
    }

    // eslint-disable-next-line no-use-before-define
    if (!(channel instanceof Channel)) {
      throw new ERR_INVALID_ARG_TYPE('channel', ['Channel'], channel);
    }
    if (typeof onComplete !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('onComplete', ['function'], onComplete);
    }

    function onMessage(message) {
      // A span message may be pre or post serialization so identify either
      if (message instanceof SpanMessage || message.type === 'span') {
        let messages = MapPrototypeGet(spans, message.id);
        if (!messages) {
          messages = [];
          MapPrototypeSet(spans, message.id, messages);
        }

        ArrayPrototypePush(messages, map(message));

        if (message.action === 'end') {
          onComplete(messages);
          MapPrototypeDelete(spans, message.id);
        }
      }
    }

    channel.subscribe(onMessage);

    return () => {
      spans.clear();
      channel.unsubscribe(onMessage);
    };
  }
}

// TODO(qard): should there be a C++ channel interface?
class ActiveChannel {
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

  span(data) {
    return new Span(this, data);
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

  span() {
    return new InactiveSpan();
  }
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
  Channel,
  Span
};
