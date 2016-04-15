'use strict';

var domain;

function EventEmitter() {
  EventEmitter.init.call(this);
}
module.exports = EventEmitter;

// Backwards-compat with node 0.10.x
EventEmitter.EventEmitter = EventEmitter;

EventEmitter.usingDomains = false;

EventEmitter.prototype.domain = undefined;
EventEmitter.prototype._events = undefined;
EventEmitter.prototype._maxListeners = undefined;

// By default EventEmitters will print a warning if more than 10 listeners are
// added to it. This is a useful default which helps finding memory leaks.
var defaultMaxListeners = 10;

Object.defineProperty(EventEmitter, 'defaultMaxListeners', {
  enumerable: true,
  get: function() {
    return defaultMaxListeners;
  },
  set: function(arg) {
    // force global console to be compiled.
    // see https://github.com/nodejs/node/issues/4467
    console;
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

  if (!this._events || this._events === Object.getPrototypeOf(this)._events) {
    this._events = {};
    this._eventsCount = 0;
  }

  this._maxListeners = this._maxListeners || undefined;
};

// Obviously not all Emitters should be limited to 10. This function allows
// that to be increased. Set to zero for unlimited.
EventEmitter.prototype.setMaxListeners = function setMaxListeners(n) {
  if (typeof n !== 'number' || n < 0 || isNaN(n))
    throw new TypeError('"n" argument must be a positive number');
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

// These standalone emit* functions are used to optimize calling of event
// handlers for fast cases because emit() itself often has a variable number of
// arguments and can be deoptimized because of that. These functions always have
// the same number of arguments and thus do not get deoptimized, so the code
// inside them can execute faster.
function emitNone(handler, isFn, self) {
  if (isFn)
    handler.call(self);
  else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    for (var i = 0; i < len; ++i)
      listeners[i].call(self);
  }
}
function emitNoneOnce(handler, isFn, self, ev) {
  var fired;
  if (isFn) {
    fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self);
  } else {
    fired = false;
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var once;
    for (var i = 0; i < len; ++i) {
      handler = listeners[i];
      once = handler.once;
      if (once !== undefined) {
        fired = handler.fired;
        if (!fired)
          handler.fired = true;
        handler = once;
        self.removeListener(ev, handler);
        if (fired)
          continue;
      }
      handler.call(self);
    }
  }
}
function emitOne(handler, isFn, self, arg1) {
  if (isFn)
    handler.call(self, arg1);
  else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    for (var i = 0; i < len; ++i)
      listeners[i].call(self, arg1);
  }
}
function emitOneOnce(handler, isFn, self, ev, arg1) {
  var fired;
  if (isFn) {
    fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self, arg1);
  } else {
    fired = false;
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var once;
    for (var i = 0; i < len; ++i) {
      handler = listeners[i];
      once = handler.once;
      if (once) {
        fired = handler.fired;
        if (!fired)
          handler.fired = true;
        handler = once;
        self.removeListener(ev, handler);
        if (fired)
          continue;
      }
      handler.call(self, arg1);
    }
  }
}
function emitTwo(handler, isFn, self, arg1, arg2) {
  if (isFn)
    handler.call(self, arg1, arg2);
  else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    for (var i = 0; i < len; ++i)
      listeners[i].call(self, arg1, arg2);
  }
}
function emitTwoOnce(handler, isFn, self, ev, arg1, arg2) {
  var fired;
  if (isFn) {
    fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self, arg1, arg2);
  } else {
    fired = false;
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var once;
    for (var i = 0; i < len; ++i) {
      handler = listeners[i];
      once = handler.once;
      if (once) {
        fired = handler.fired;
        if (!fired)
          handler.fired = true;
        handler = once;
        self.removeListener(ev, handler);
        if (fired)
          continue;
      }
      handler.call(self, arg1, arg2);
    }
  }
}
function emitThree(handler, isFn, self, arg1, arg2, arg3) {
  if (isFn)
    handler.call(self, arg1, arg2, arg3);
  else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    for (var i = 0; i < len; ++i)
      listeners[i].call(self, arg1, arg2, arg3);
  }
}
function emitThreeOnce(handler, isFn, self, ev, arg1, arg2, arg3) {
  var fired;
  if (isFn) {
    fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self, arg1, arg2, arg3);
  } else {
    fired = false;
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var once;
    for (var i = 0; i < len; ++i) {
      handler = listeners[i];
      once = handler.once;
      if (once) {
        fired = handler.fired;
        if (!fired)
          handler.fired = true;
        handler = once;
        self.removeListener(ev, handler);
        if (fired)
          continue;
      }
      handler.call(self, arg1, arg2, arg3);
    }
  }
}
function emitMany(handler, isFn, self, args) {
  if (isFn)
    handler.apply(self, args);
  else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    for (var i = 0; i < len; ++i)
      listeners[i].apply(self, args);
  }
}
function emitManyOnce(handler, isFn, self, ev, args) {
  var fired;
  if (isFn) {
    fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.apply(self, args);
  } else {
    fired = false;
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var once;
    for (var i = 0; i < len; ++i) {
      handler = listeners[i];
      once = handler.once;
      if (once) {
        fired = handler.fired;
        if (!fired)
          handler.fired = true;
        handler = once;
        self.removeListener(ev, handler);
        if (fired)
          continue;
      }
      handler.apply(self, args);
    }
  }
}

EventEmitter.prototype.emit = function emit(type) {
  var i, j;
  var isArray = false;
  var useOnce = false;
  var needDomainExit = false;
  var doError = (type === 'error');

  const events = this._events;
  if (events !== undefined)
    doError = (doError && events.error === undefined);
  else if (!doError)
    return false;

  const domain = this.domain;

  // If there is no 'error' event listener then throw.
  if (doError) {
    var er = arguments[1];
    if (domain) {
      if (!er)
        er = new Error('Uncaught, unspecified "error" event');
      er.domainEmitter = this;
      er.domain = domain;
      er.domainThrown = false;
      domain.emit('error', er);
    } else if (er instanceof Error) {
      throw er; // Unhandled 'error' event
    } else {
      // At least give some kind of context to the user
      var err = new Error('Uncaught, unspecified "error" event. (' + er + ')');
      err.context = er;
      throw err;
    }
    return false;
  }

  const handler = events[type];

  if (handler === undefined)
    return false;

  if (domain != null && this !== process) {
    domain.enter();
    needDomainExit = true;
  }

  const len = arguments.length;

  if (typeof handler !== 'function') {
    const onceCount = handler.onceCount;
    isArray = (typeof handler.once !== 'function');
    useOnce = (!isArray || onceCount > 0);
  }

  switch (len) {
    // fast cases
    case 1:
      if (!useOnce)
        emitNone(handler, !isArray, this);
      else
        emitNoneOnce(handler, !isArray, this, type);
      break;
    case 2:
      if (!useOnce)
        emitOne(handler, !isArray, this, arguments[1]);
      else
        emitOneOnce(handler, !isArray, this, type, arguments[1]);
      break;
    case 3:
      if (!useOnce)
        emitTwo(handler, !isArray, this, arguments[1], arguments[2]);
      else
        emitTwoOnce(handler, !isArray, this, type, arguments[1], arguments[2]);
      break;
    case 4:
      if (!useOnce) {
        emitThree(handler, !isArray, this, arguments[1], arguments[2],
                  arguments[3]);
      } else {
        emitThreeOnce(handler, !isArray, this, type, arguments[1], arguments[2],
                      arguments[3]);
      }
      break;
    // slower
    default:
      var args = new Array(len - 1);
      for (i = 1, j = 0; i < len; ++i, ++j)
        args[j] = arguments[i];
      if (!useOnce)
        emitMany(handler, !isArray, this, args);
      else
        emitManyOnce(handler, !isArray, this, type, args);
  }

  if (needDomainExit)
    domain.exit();

  return true;
};

EventEmitter.prototype.on = function on(type, listener) {
  var events;
  var existing;

  if (typeof listener !== 'function')
    throw new TypeError('"listener" argument must be a function');

  events = this._events;
  if (!events) {
    events = this._events = {};
    this._eventsCount = 0;
  } else {
    // To avoid recursion in the case that type === "newListener"! Before
    // adding it to the listeners, first emit "newListener".
    if (events.newListener) {
      this.emit('newListener', type, listener);

      // Re-assign `events` because a newListener handler could have caused
      // `this._events` to be assigned to a new object
      events = this._events;
    }
    existing = events[type];
  }

  if (!existing) {
    // Optimize the case of one listener. Don't need the extra array object.
    existing = events[type] = listener;
    ++this._eventsCount;
  } else {
    if (typeof existing === 'function') {
      // Adding the second element, need to change to array.
      existing = events[type] = [existing, listener];
    } else if (typeof existing.once === 'function') {
      // Adding the second element, need to change to array. Existing listener
      // was a one-time listener.
      existing = events[type] = [existing, listener];
      existing.onceCount = 1;
    } else {
      // If we've already got an array, just append.
      existing.push(listener);
    }

    // Check for listener leak
    if (!existing.warned) {
      const m = $getMaxListeners(this);
      if (m > 0 && existing.length > m) {
        existing.warned = true;
        process.emitWarning('Possible EventEmitter memory leak detected. ' +
                            `${existing.length} ${type} listeners added. ` +
                            'Use emitter.setMaxListeners() to increase limit');
      }
    }
  }

  return this;
};

EventEmitter.prototype.addListener = EventEmitter.prototype.on;

EventEmitter.prototype.once = function once(type, listener) {
  var events;
  var existing;

  if (typeof listener !== 'function')
    throw new TypeError('"listener" argument must be a function');

  events = this._events;
  if (!events) {
    events = this._events = {};
    this._eventsCount = 0;
  } else {
    // To avoid recursion in the case that type === "newListener"! Before
    // adding it to the listeners, first emit "newListener".
    if (events.newListener) {
      this.emit('newListener', type, listener);

      // Re-assign `events` because a newListener handler could have caused
      // `this._events` to be assigned to a new object
      events = this._events;
    }
    existing = events[type];
  }

  if (!existing) {
    // Optimize the case of one listener. Don't need the extra array object.
    existing = events[type] = { once: listener, fired: false };
    ++this._eventsCount;
  } else {
    if (typeof existing === 'function' || typeof existing.once === 'function') {
      // Adding the second element, need to change to array.
      existing = events[type] = [
        existing,
        { once: listener, fired: false }
      ];
      existing.onceCount = (typeof existing.once === 'function' ? 2 : 1);
    } else {
      // If we've already got an array, just append.
      existing.push({ once: listener, fired: false });
      if (existing.onceCount === undefined)
        existing.onceCount = 1;
      else
        ++existing.onceCount;
    }

    // Check for listener leak
    if (!existing.warned) {
      const m = $getMaxListeners(this);
      if (m > 0 && existing.length > m) {
        existing.warned = true;
        if (!internalUtil)
          internalUtil = require('internal/util');

        internalUtil.error('warning: possible EventEmitter memory ' +
                           'leak detected. %d %s listeners added. ' +
                           'Use emitter.setMaxListeners() to increase limit.',
                           existing.length, type);
        console.trace();
      }
    }
  }

  return this;
};

// emits a 'removeListener' event iff the listener was removed
EventEmitter.prototype.removeListener = removeListener;
function removeListener(type, listener) {
  if (typeof listener !== 'function')
    throw new TypeError('"listener" argument must be a function');

  const events = this._events;
  if (events === undefined)
    return this;

  const list = events[type];
  if (list === undefined)
    return this;

  if ((list.once || list) === listener) {
    if (--this._eventsCount === 0)
      this._events = {};
    else {
      delete events[type];
      if (events.removeListener !== undefined)
        this.emit('removeListener', type, listener);
    }
  } else if (typeof list !== 'function') {
    var position = -1;
    var isOnce = false;
    var i;
    const len = list.length;

    if (list.onceCount > 0) {
      for (i = len; i-- > 0;) {
        const curListener = list[i];
        if ((curListener.once || curListener) === listener) {
          isOnce = (curListener !== listener);
          position = i;
          break;
        }
      }
    } else {
      for (i = len; i-- > 0;) {
        const curListener = list[i];
        if (curListener === listener) {
          position = i;
          break;
        }
      }
    }

    if (position < 0)
      return this;

    if (len > 2) {
      if (isOnce)
        --list.onceCount;
      if (position === len - 1)
        list.pop();
      else if (position === 0)
        list.shift();
      else
        spliceOne(list, position, len);
    } else if (position === 0) {
      events[type] = list[1];
    } else {
      events[type] = list[0];
    }

    if (events.removeListener)
      this.emit('removeListener', type, listener);
  }

  return this;
}

EventEmitter.prototype.removeAllListeners = function removeAllListeners(type) {
  const events = this._events;
  if (events === undefined)
    return this;

  // not listening for removeListener, no need to emit
  if (events.removeListener === undefined) {
    if (type === undefined) {
      this._events = {};
      this._eventsCount = 0;
    } else if (events[type] !== undefined) {
      if (--this._eventsCount === 0)
        this._events = {};
      else
        delete events[type];
    }
    return this;
  }

  // emit removeListener for all listeners on all events
  if (type === undefined) {
    const keys = Object.keys(events);
    for (var i = 0; i < keys.length; ++i) {
      const key = keys[i];
      if (key === 'removeListener')
        continue;
      this.removeAllListeners(key);
    }
    this.removeAllListeners('removeListener');
    this._events = {};
    this._eventsCount = 0;
    return this;
  }

  var listeners = events[type];
  if (listeners !== undefined) {
    if (typeof listeners === 'function') {
      this.removeListener(type, listeners);
    } else if (typeof listeners.once === 'function') {
      this.removeListener(type, listeners.once);
    } else {
      // LIFO order
      for (i = listeners.length; i-- > 0;) {
        const listener = listeners[i];
        if (typeof listener === 'function')
          this.removeListener(type, listener);
        else
          this.removeListener(type, listener.once);
      }
    }
  }

  return this;
};

EventEmitter.prototype.listeners = function listeners(type) {
  const events = this._events;
  if (events !== undefined) {
    const listener = events[type];
    if (listener instanceof Array) {
      return (!listener.onceCount
              ? arrayClone(listener, listener.length)
              : onceClone(listener));
    } else if (listener !== undefined) {
      return [listener.once || listener];
    }
  }
  return [];
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
    const listener = events[type];
    if (listener instanceof Array)
      return listener.length;
    else if (listener !== undefined)
      return 1;
  }
  return 0;
}

EventEmitter.prototype.eventNames = function eventNames() {
  if (this._eventsCount > 0) {
    const events = this._events;
    return Object.keys(events).concat(
      Object.getOwnPropertySymbols(events));
  }
  return [];
};

// About 1.5x faster than the two-arg version of Array#splice().
function spliceOne(list, index, len) {
  for (var i = index, k = i + 1, n = len || list.length; k < n; i += 1, k += 1)
    list[i] = list[k];
  list.pop();
}


function onceClone(arr) {
  const len = arr.length;
  const ret = new Array(len);
  for (var i = 0; i < len; ++i) {
    const fn = arr[i];
    ret[i] = fn.once || fn;
  }
  return ret;
}

function arrayClone(arr, len) {
  const copy = new Array(len);
  while (len--)
    copy[len] = arr[len];
  return copy;
}
