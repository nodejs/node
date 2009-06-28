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

emitter.listeners = function (type, listener) {
  if (!this._events) this._events = {};
  if (!this._events.hasOwnProperty(type)) this._events[type] = [];
  return this._events[type];
};

/* This function is called often from C++. 
 * See events.cc 
 */ 
emitter.emit = function (type, args) {
  if (!this._events) return;
  if (!this._events.hasOwnProperty(type)) return;
  var listeners = this._events[type];
  var length = listeners.length;
  for (var i = 0; i < length; i++) {
    listeners[i].apply(this, args);
  }
};

// node.Promise is defined in src/events.cc
var promise = node.Promise.prototype;

promise.addCallback = function (listener) {
  this.addListener("Success", listener);
};

promise.addErrback = function (listener) {
  this.addListener("Error", listener);
};

})(); // end anonymous namespace
