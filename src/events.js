(function () {

// node.EventEmitter is defined in src/events.cc
var emitter = node.EventEmitter.prototype;

emitter.addListener = function (type, listener) {
  if (!this._events) this._events = {};
  if (!this._events.hasOwnProperty(type)) this._events[type] = [];
  this._events[type].push(listener);
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
  if (this["on" + type] instanceof Function) {
    this["on" + type].apply(this, args);
  }
  if (!this._events) return;
  if (!this._events.hasOwnProperty(type)) return;
  for (var i = 0; i < this._events[type].length; i++) {
    var listener = this._events[type][i];
    listener.apply(this, args);
  }
};

})(); // end annonymous namespace
