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
EventEmitter.defaultMaxListeners = 10;

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
    throw new TypeError('n must be a positive number');
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
  if (isFn) {
    var fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self);
  } else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var fired = false;
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
  if (isFn) {
    var fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self, arg1);
  } else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var fired = false;
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
  if (isFn) {
    var fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self, arg1, arg2);
  } else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var fired = false;
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
  if (isFn) {
    var fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.call(self, arg1, arg2, arg3);
  } else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var fired = false;
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
  if (isFn) {
    var fired = handler.fired;
    if (!fired)
      handler.fired = true;
    handler = handler.once;
    self.removeListener(ev, handler);
    if (fired)
      return;
    handler.apply(self, args);
  } else {
    var len = handler.length;
    var listeners = arrayClone(handler, len);
    var fired = false;
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
  var er, handler, len, args, i, events, domain, onceCount;
  var isArray = false;
  var useOnce = false;
  var needDomainExit = false;
  var doError = (type === 'error');

  events = this._events;
  if (events)
    doError = (doError && events.error == null);
  else if (!doError)
    return false;

  domain = this.domain;

  // If there is no 'error' event listener then throw.
  if (doError) {
    er = arguments[1];
    if (domain) {
      if (!er)
        er = new Error('Uncaught, unspecified "error" event.');
      er.domainEmitter = this;
      er.domain = domain;
      er.domainThrown = false;
      domain.emit('error', er);
    } else if (er instanceof Error) {
      throw er; // Unhandled 'error' event
    } else {
      throw Error('Uncaught, unspecified "error" event.');
    }
    return false;
  }

  handler = events[type];

  if (!handler)
    return false;

  if (domain && this !== process) {
    domain.enter();
    needDomainExit = true;
  }

  len = arguments.length;

  if (typeof handler !== 'function') {
    onceCount = handler.onceCount;
    useOnce = (onceCount !== 0);
    isArray = (onceCount !== undefined);
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
      args = new Array(len - 1);
      for (i = 1; i < len; i++)
        args[i - 1] = arguments[i];
      if (!useOnce)
        emitMany(handler, !isArray, this, args);
      else
        emitManyOnce(handler, !isArray, this, type, args);
  }

  if (needDomainExit)
    domain.exit();

  return true;
};

EventEmitter.prototype._addListener =
    function _addListener(type, listener, once) {
      var m;
      var events;
      var existing;

      if (typeof listener !== 'function')
        throw new TypeError('listener must be a function');

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
        if (once)
          existing = events[type] = { once: listener, fired: false };
        else
          existing = events[type] = listener;
        ++this._eventsCount;
      } else {
        var existingOnce = existing.once;
        if (typeof existing === 'function' || existingOnce) {
          // Adding the second element, need to change to array.
          if (once) {
            existing = events[type] = [
              existing,
              { once: listener, fired: false }
            ];
            existing.onceCount = (existingOnce ? 2 : 1);
          } else {
            existing = events[type] = [existing, listener];
            existing.onceCount = (existingOnce ? 1 : 0);
          }
        } else {
          // If we've already got an array, just append.
          if (once) {
            existing.push({ once: listener, fired: false });
            ++existing.onceCount;
          } else
            existing.push(listener);
        }

        // Check for listener leak
        if (!existing.warned) {
          m = $getMaxListeners(this);
          if (m && m > 0 && existing.length > m) {
            existing.warned = true;
            console.error('(node) warning: possible EventEmitter memory ' +
                          'leak detected. %d %s listeners added. ' +
                          'Use emitter.setMaxListeners() to increase limit.',
                          existing.length, type);
            console.trace();
          }
        }
      }

      return this;
    };

EventEmitter.prototype.on = function on(type, listener) {
  if (!this._addListener) {
    return EventEmitter.prototype._addListener.call(this, type, listener,
                                                    false);
  }
  return this._addListener(type, listener, false);
};

EventEmitter.prototype.addListener = EventEmitter.prototype.on;

EventEmitter.prototype.once = function once(type, listener) {
  if (!this._addListener) {
    return EventEmitter.prototype._addListener.call(this, type, listener,
                                                    true);
  }
  return this._addListener(type, listener, true);
};

// emits a 'removeListener' event iff the listener was removed
EventEmitter.prototype.removeListener =
    function removeListener(type, listener) {
      var list, events, position, i, curListener, len, once, isOnce;

      if (typeof listener !== 'function')
        throw new TypeError('listener must be a function');

      events = this._events;
      if (!events)
        return this;

      list = events[type];
      if (!list)
        return this;

      if ((list.once || list) === listener) {
        if (--this._eventsCount === 0)
          this._events = {};
        else {
          delete events[type];
          if (events.removeListener)
            this.emit('removeListener', type, listener);
        }
      } else if (typeof list !== 'function') {
        position = -1;
        len = list.length;
        isOnce = false;

        if (list.onceCount > 0) {
          for (i = len; i-- > 0;) {
            curListener = list[i];
            once = curListener.once;
            if ((once || curListener) === listener) {
              isOnce = (once === listener);
              position = i;
              break;
            }
          }
        } else {
          for (i = len; i-- > 0;) {
            curListener = list[i];
            if (curListener === listener) {
              position = i;
              break;
            }
          }
        }

        if (position < 0)
          return this;

        if (len !== 2) {
          if (isOnce)
            --list.onceCount;
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
    };

EventEmitter.prototype.removeAllListeners =
    function removeAllListeners(type) {
      var listeners, events;

      events = this._events;
      if (!events)
        return this;

      // not listening for removeListener, no need to emit
      if (!events.removeListener) {
        if (arguments.length === 0) {
          this._events = {};
          this._eventsCount = 0;
        } else if (events[type]) {
          if (--this._eventsCount === 0)
            this._events = {};
          else
            delete events[type];
        }
        return this;
      }

      // emit removeListener for all listeners on all events
      if (arguments.length === 0) {
        var keys = Object.keys(events);
        for (var i = 0, key; i < keys.length; ++i) {
          key = keys[i];
          if (key === 'removeListener') continue;
          this.removeAllListeners(key);
        }
        this.removeAllListeners('removeListener');
        this._events = {};
        this._eventsCount = 0;
        return this;
      }

      listeners = events[type];
      if (!listeners)
        return this;

      if (typeof (listeners.once || listeners) === 'function') {
        this.removeListener(type, listeners);
      } else if (listeners) {
        // LIFO order
        for (i = listeners.length; i-- > 0;) {
          this.removeListener(type, listeners[i]);
        }
      }

      return this;
    };

EventEmitter.prototype.listeners = function listeners(type) {
  var evlistener;
  var ret;
  var events = this._events;

  if (!events)
    ret = [];
  else {
    evlistener = events[type];
    if (!evlistener)
      ret = [];
    else if (evlistener.onceCount === 0)
      ret = arrayClone(evlistener);
    else {
      var once = evlistener.once;
      if (typeof (once || evlistener) === 'function')
        ret = [once || evlistener];
      else
        ret = onceClone(evlistener);
    }
  }

  return ret;
};

EventEmitter.listenerCount = function(emitter, type) {
  var evlistener;
  var ret = 0;
  var events = emitter._events;

  if (events) {
    evlistener = events[type];
    if (typeof evlistener === 'function')
      ret = 1;
    else if (evlistener)
      ret = evlistener.length;
  }

  return ret;
};

// About 1.5x faster than the two-arg version of Array#splice().
function spliceOne(list, index, len) {
  for (var i = index, k = i + 1, n = len || list.length; k < n; i += 1, k += 1)
    list[i] = list[k];
  list.pop();
}


function onceClone(arr) {
  var len = arr.length;
  var ret = new Array(len);
  for (var i = 0, fn; i < len; i += 1) {
    fn = arr[i];
    ret[i] = fn.once || fn;
  }
  return ret;
}

function arrayClone(arr, len) {
  var ret;
  if (len === undefined)
    len = arr.length;
  if (len >= 50)
    ret = arr.slice();
  else {
    ret = new Array(len);
    for (var i = 0; i < len; i += 1)
      ret[i] = arr[i];
  }
  return ret;
}
