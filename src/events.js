(function () {

// node.EventEmitter is defined in src/events.cc
var emitter = node.EventEmitter.prototype;

emitter.addListener = function (type, listener) {
  if (listener instanceof Function) {
    if (!this._events) this._events = {};
    if (!this._events.hasOwnProperty(type)) this._events[type] = [];
    this._events[type].push(listener);
  }
};

emitter.listeners = function (type) {
  if (!this._events) this._events = {};
  if (!this._events.hasOwnProperty(type)) this._events[type] = [];
  return this._events[type];
};

// This function is called often from C++. 
// FIXME there is a counterpart for this function in C++
// both must have the same behavior.
// See events.cc 
emitter.emit = function (type, args) {
  if (!this._events) return;
  if (!this._events.hasOwnProperty(type)) return;

  var listeners = this._events[type];

  for (var i = 0; i < listeners.length; i++) {
    listeners[i].apply(this, args);
  }
};

// node.Promise is defined in src/events.cc
var promise = node.Promise.prototype;

promise.addCallback = function (listener) {
  this.addListener("success", listener);
  return this;
};

promise.addErrback = function (listener) {
  this.addListener("error", listener);
  return this;
};

promise.emitSuccess = function (args) {
  this.emit("success", args);
};

promise.emitError = function (args) {
  this.emit("error", args);
};

})(); // end anonymous namespace
