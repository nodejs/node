// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

var domain;
var spliceOne;

function EventEmitter() {
  EventEmitter.init.call(this);
}
module.exports = EventEmitter;

// Backwards-compat with node 0.10.x
EventEmitter.EventEmitter = EventEmitter;

EventEmitter.usingDomains = false;

EventEmitter.prototype.domain = undefined;
EventEmitter.prototype._events = undefined;
EventEmitter.prototype._eventsCount = 0;
EventEmitter.prototype._maxListeners = undefined;

// By default EventEmitters will print a warning if more than 10 listeners are
// added to it. This is a useful default which helps finding memory leaks.
var defaultMaxListeners = 10;

var errors;
function lazyErrors() {
  if (errors === undefined)
    errors = require('internal/errors');
  return errors;
}

Object.defineProperty(EventEmitter, 'defaultMaxListeners', {
  enumerable: true,
  get: function() {
    return defaultMaxListeners;
  },
  set: function(arg) {
    // check whether the input is a positive number (whose value is zero or
    // greater and not a NaN).
    if (typeof arg !== 'number' || arg < 0 || arg !== arg) {
      const errors = lazyErrors();
      throw new errors.TypeError('ERR_OUT_OF_RANGE', 'defaultMaxListeners');
    }
    defaultMaxListeners = arg;
  }
});

EventEmitter.init = function() {
  this.domain = null;
  if (EventEmitter.usingDomains) {
    // if there is an active domain, then attach to it.
    domain = domain || require('domain');
    if (domain.active && !(this instanceof domain.Domain)) {
      this.domain = domain.active;
    }
  }

  if (this._events === undefined ||
      this._events === Object.getPrototypeOf(this)._events) {
    this._events = Object.create(null);
    this._eventsCount = 0;
  }

  if (!this._maxListeners)
    this._maxListeners = undefined;
};

// Obviously not all Emitters should be limited to 10. This function allows
// that to be increased. Set to zero for unlimited.
EventEmitter.prototype.setMaxListeners = function setMaxListeners(n) {
  if (typeof n !== 'number' || n < 0 || isNaN(n)) {
    const errors = lazyErrors();
    throw new errors.TypeError('ERR_OUT_OF_RANGE', 'n');
  }
  this._maxListeners = n;
  return this;
};

function $getMaxListeners(that) {
  if (that._maxListeners === undefined)
    return EventEmitter.defaultMaxListeners;
  return that._maxListeners;
}

EventEmitter.prototype.getMaxListeners = function getMaxListeners() {
  return $getMaxListeners(this);
};

EventEmitter.prototype.emit = function emit(type, ...args) {
  let doError = (type === 'error');

  const events = this._events;
  if (events !== undefined)
    doError = (doError && events.error === undefined);
  else if (!doError)
    return false;

  const domain = this.domain;

  // If there is no 'error' event listener then throw.
  if (doError) {
    let er;
    if (args.length > 0)
      er = args[0];
    if (domain !== null && domain !== undefined) {
      if (!er) {
        const errors = lazyErrors();
        er = new errors.Error('ERR_UNHANDLED_ERROR');
      }
      if (typeof er === 'object' && er !== null) {
        er.domainEmitter = this;
        er.domain = domain;
        er.domainThrown = false;
      }
      domain.emit('error', er);
    } else if (er instanceof Error) {
      throw er; // Unhandled 'error' event
    } else {
      // At least give some kind of context to the user
      const errors = lazyErrors();
      const err = new errors.Error('ERR_UNHANDLED_ERROR', er);
      err.context = er;
      throw err;
    }
    return false;
  }

  const handler = events[type];

  if (handler === undefined)
    return false;

  let needDomainExit = false;
  if (domain !== null && domain !== undefined && this !== process) {
    domain.enter();
    needDomainExit = true;
  }

  if (typeof handler === 'function') {
    Reflect.apply(handler, this, args);
  } else {
    const prevEmitting = handler.emitting;
    handler.emitting = true;
    for (var i = 0; i < handler.length; ++i)
      Reflect.apply(handler[i], this, args);
    handler.emitting = prevEmitting;
  }

  if (needDomainExit)
    domain.exit();

  return true;
};

function _addListener(target, type, listener, prepend) {
  if (typeof listener !== 'function') {
    const errors = lazyErrors();
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'listener', 'Function');
  }

  var events = target._events;
  var list;
  if (events === undefined) {
    events = target._events = Object.create(null);
    target._eventsCount = 0;
  } else {
    // To avoid recursion in the case that type === "newListener"! Before
    // adding it to the listeners, first emit "newListener".
    if (events.newListener !== undefined) {
      target.emit('newListener', type, listener.listener || listener);

      // Re-assign `events` because a newListener handler could have caused the
      // this._events to be assigned to a new object
      events = target._events;
    }
    list = events[type];
  }

  if (list === undefined) {
    // Optimize the case of one listener. Don't need the extra array object.
    events[type] = listener;
    ++target._eventsCount;
    return target;
  }

  if (typeof list === 'function') {
    // Adding the second element, need to change to array.
    list = events[type] = prepend ? [listener, list] : [list, listener];
  } else if (list.emitting) {
    const { warned } = list;
    list = events[type] =
      arrayCloneWithElement(list, listener, prepend ? 1 : 0);
    if (warned) {
      list.warned = true;
      return target;
    }
  } else if (prepend) {
    list.unshift(listener);
  } else {
    list.push(listener);
  }

  // Check for listener leak
  const m = $getMaxListeners(target);
  if (m > 0 && list.length > m && !list.warned) {
    list.warned = true;
    // No error code for this since it is a Warning
    const w = new Error('Possible EventEmitter memory leak detected. ' +
                        `${list.length} ${String(type)} listeners ` +
                        'added. Use emitter.setMaxListeners() to ' +
                        'increase limit');
    w.name = 'MaxListenersExceededWarning';
    w.emitter = target;
    w.type = type;
    w.count = list.length;
    process.emitWarning(w);
  }

  return target;
}

EventEmitter.prototype.addListener = function addListener(type, listener) {
  return _addListener(this, type, listener, false);
};

EventEmitter.prototype.on = EventEmitter.prototype.addListener;

EventEmitter.prototype.prependListener =
    function prependListener(type, listener) {
      return _addListener(this, type, listener, true);
    };

function onceWrapper() {
  if (!this.fired) {
    this.target.removeListener(this.type, this.wrapFn);
    this.fired = true;
    Reflect.apply(this.listener, this.target, arguments);
  }
}

function _onceWrap(target, type, listener) {
  var state = { fired: false, wrapFn: undefined, target, type, listener };
  var wrapped = onceWrapper.bind(state);
  wrapped.listener = listener;
  state.wrapFn = wrapped;
  return wrapped;
}

EventEmitter.prototype.once = function once(type, listener) {
  if (typeof listener !== 'function') {
    const errors = lazyErrors();
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'listener', 'Function');
  }
  this.on(type, _onceWrap(this, type, listener));
  return this;
};

EventEmitter.prototype.prependOnceListener =
    function prependOnceListener(type, listener) {
      if (typeof listener !== 'function') {
        const errors = lazyErrors();
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'listener',
                                   'Function');
      }
      this.prependListener(type, _onceWrap(this, type, listener));
      return this;
    };

// Emits a 'removeListener' event if and only if the listener was removed.
EventEmitter.prototype.removeListener =
    function removeListener(type, listener) {
      if (typeof listener !== 'function') {
        const errors = lazyErrors();
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'listener',
                                   'Function');
      }

      const events = this._events;
      if (events === undefined)
        return this;

      const list = events[type];
      if (list === undefined)
        return this;

      if (list === listener || list.listener === listener) {
        events[type] = undefined;
        --this._eventsCount;
        if (events.removeListener !== undefined)
          this.emit('removeListener', type, list.listener || listener);
        return this;
      }

      if (typeof list !== 'function') {
        let position = -1;

        for (var i = list.length - 1; i >= 0; --i) {
          const l = list[i];
          if (l === listener || l.listener === listener) {
            if (l.listener)
              listener = l.listener;
            position = i;
            break;
          }
        }

        if (position < 0)
          return this;

        if (list.length === 2)
          events[type] = list[position ? 0 : 1];
        else if (list.emitting) {
          const { warned } = list;
          events[type] = sliceOne(list, position);
          if (warned)
            events[type].warned = true;
        } else if (position === 0)
          list.shift();
        else if (position === list.length - 1)
          list.pop();
        else {
          if (spliceOne === undefined)
            spliceOne = require('internal/util').spliceOne;
          spliceOne(list, position);
        }

        if (events.removeListener !== undefined)
          this.emit('removeListener', type, listener);
      }

      return this;
    };

EventEmitter.prototype.removeAllListeners =
    function removeAllListeners(type) {
      var listeners, events, i;

      events = this._events;
      if (events === undefined)
        return this;

      // not listening for removeListener, no need to emit
      if (events.removeListener === undefined) {
        if (arguments.length === 0) {
          this._events = Object.create(null);
          this._eventsCount = 0;
        } else if (events[type] !== undefined) {
          events[type] = undefined;
          --this._eventsCount;
        }
        return this;
      }

      // emit removeListener for all listeners on all events
      if (arguments.length === 0) {
        const keys = Reflect.ownKeys(events);
        var key;
        for (i = 0; i < keys.length; ++i) {
          key = keys[i];
          if (key === 'removeListener' || events[key] === undefined)
            continue;
          this.removeAllListeners(key);
        }
        this.removeAllListeners('removeListener');
        return this;
      }

      listeners = events[type];

      if (typeof listeners === 'function') {
        this.removeListener(type, listeners);
      } else if (listeners !== undefined) {
        // LIFO order
        for (i = listeners.length - 1; i >= 0; --i) {
          this.removeListener(type, listeners[i]);
        }
      }

      return this;
    };

EventEmitter.prototype.listeners = function listeners(type) {
  const events = this._events;

  if (events === undefined)
    return [];

  const evlistener = events[type];
  if (evlistener === undefined)
    return [];

  if (typeof evlistener === 'function')
    return [evlistener.listener || evlistener];

  return unwrapListeners(evlistener);
};

EventEmitter.listenerCount = function(emitter, type) {
  if (typeof emitter.listenerCount === 'function') {
    return emitter.listenerCount(type);
  } else {
    return listenerCount.call(emitter, type);
  }
};

EventEmitter.prototype.listenerCount = listenerCount;
function listenerCount(type) {
  const events = this._events;

  if (events !== undefined) {
    const evlistener = events[type];

    if (typeof evlistener === 'function') {
      return 1;
    } else if (evlistener !== undefined) {
      return evlistener.length;
    }
  }

  return 0;
}

EventEmitter.prototype.eventNames = function eventNames() {
  const count = this._eventsCount;
  if (count === 0)
    return [];

  const events = this._events;
  const actualEventNames = new Array(count);
  var j = 0;
  for (var key in events) {
    if (events[key] !== undefined) {
      actualEventNames[j] = key;
      j++;
    }
  }

  // We must have Symbols to fill in
  if (j < count) {
    const symbols = Object.getOwnPropertySymbols(events);
    for (var i = 0; i < symbols.length; ++i) {
      key = symbols[i];
      if (events[key] !== undefined) {
        actualEventNames[j] = key;
        j++;
      }
    }
  }

  return actualEventNames;
};

function arrayCloneWithElement(arr, element, prepend) {
  const len = arr.length;
  const copy = new Array(len + 1);
  for (var i = 0 + prepend; i < len + prepend; ++i)
    copy[i] = arr[i - prepend];
  copy[prepend ? 0 : len] = element;
  return copy;
}

function sliceOne(arr, index) {
  const len = arr.length - 1;
  const copy = new Array(len);
  for (var i = 0, offset = 0; i < len; ++i) {
    if (i === index)
      offset = 1;
    copy[i] = arr[i + offset];
  }
  return copy;
}

function unwrapListeners(arr) {
  const ret = new Array(arr.length);
  for (var i = 0; i < ret.length; ++i) {
    ret[i] = arr[i].listener || arr[i];
  }
  return ret;
}
