(function () {

// node.EventEmitter is defined in src/events.cc
// node.EventEmitter.prototype.emit() is also defined there.
node.EventEmitter.prototype.addListener = function (type, listener) {
  if (listener instanceof Function) {
    if (!this._events) this._events = {};
    if (!this._events.hasOwnProperty(type)) this._events[type] = [];
    this._events[type].push(listener);
  }
};

node.EventEmitter.prototype.listeners = function (type) {
  if (!this._events) this._events = {};
  if (!this._events.hasOwnProperty(type)) this._events[type] = [];
  return this._events[type];
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
