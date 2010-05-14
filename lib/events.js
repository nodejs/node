exports.EventEmitter = process.EventEmitter;


process.EventEmitter.prototype.emit = function (type) {
  // If there is no 'error' event listener then throw.
  if (type === 'error') {
    if (!this._events || !this._events.error ||
        (this._events.error instanceof Array && !this._events.error.length))
    {
      if (arguments[1] instanceof Error) {
        throw arguments[1];
      } else {
        throw new Error("Uncaught, unspecified 'error' event.");
      }
      return false;
    }
  }

  if (!this._events) return false;
  if (!this._events[type]) return false;

  if (typeof this._events[type] == 'function') {
    if (arguments.length < 3) {
      // fast case
      this._events[type].call( this
                             , arguments[1]
                             , arguments[2]
                             );
    } else {
      // slower
      var args = Array.prototype.slice.call(arguments, 1);
      this._events[type].apply(this, args);
    }
    return true;

  } else if (this._events[type] instanceof Array) {
    var args = Array.prototype.slice.call(arguments, 1);


    var listeners = this._events[type].slice(0);
    for (var i = 0, l = listeners.length; i < l; i++) {
      listeners[i].apply(this, args);
    }
    return true;

  } else {
    return false;
  }
};

// process.EventEmitter is defined in src/node_events.cc
// process.EventEmitter.prototype.emit() is also defined there.
process.EventEmitter.prototype.addListener = function (type, listener) {
  if ('function' !== typeof listener) {
    throw new Error('addListener only takes instances of Function');
  }

  if (!this._events) this._events = {};

  // To avoid recursion in the case that type == "newListeners"! Before
  // adding it to the listeners, first emit "newListeners".
  this.emit("newListener", type, listener);

  if (!this._events[type]) {
    // Optimize the case of one listener. Don't need the extra array object.
    this._events[type] = listener;
  } else if (this._events[type] instanceof Array) {
    // If we've already got an array, just append.
    this._events[type].push(listener);
  } else {
    // Adding the second element, need to change to array.
    this._events[type] = [this._events[type], listener];
  }

  return this;
};


process.EventEmitter.prototype.removeListener = function (type, listener) {
  if ('function' !== typeof listener) {
    throw new Error('removeListener only takes instances of Function');
  }

  // does not use listeners(), so no side effect of creating _events[type]
  if (!this._events || !this._events[type]) return this;

  var list = this._events[type];

  if (list instanceof Array) {
    var i = list.indexOf(listener);
    if (i < 0) return this;
    list.splice(i, 1);
  } else if (this._events[type] === listener) {
    this._events[type] = null;
  }

  return this;
};

process.EventEmitter.prototype.removeAllListeners = function (type) {
  // does not use listeners(), so no side effect of creating _events[type]
  if (type && this._events && this._events[type]) this._events[type] = null;
  return this;
};

process.EventEmitter.prototype.listeners = function (type) {
  if (!this._events) this._events = {};
  if (!this._events[type]) this._events[type] = [];
  if (!(this._events[type] instanceof Array)) {
    this._events[type] = [this._events[type]];
  }
  return this._events[type];
};
exports.Promise = function removed () {
  throw new Error(
    'Promise has been removed. See '+
    'http://groups.google.com/group/nodejs/msg/0c483b891c56fea2 for more information.');
}
process.Promise = exports.Promise;
