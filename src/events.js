(function () {

// node.EventEmitter is defined in src/events.cc
// node.EventEmitter.prototype.emit() is also defined there.
node.EventEmitter.prototype.addListener = function (type, listener) {
  if (listener instanceof Function) {
    if (!this._events) this._events = {};
    if (!this._events.hasOwnProperty(type)) this._events[type] = [];
    // To avoid recursion in the case that type == "newListeners"! Before
    // adding it to the listeners, first emit "newListeners".
    this.emit("newListener", type, listener);
    this._events[type].push(listener);
  }
  return this;
};

node.EventEmitter.prototype.listeners = function (type) {
  if (!this._events) this._events = {};
  if (!this._events.hasOwnProperty(type)) this._events[type] = [];
  return this._events[type];
};

// node.Promise is defined in src/events.cc

node.Promise.prototype.addCallback = function (listener) {
  this.addListener("success", listener);
  return this;
};

node.Promise.prototype.addErrback = function (listener) {
  this.addListener("error", listener);
  return this;
};

node.Promise.prototype.wait = function () {
  var ret;
  var had_error = false;
  this.addCallback(function () {
        if (arguments.length == 1) {
          ret = arguments[0];
        } else if (arguments.length > 1) {
          ret = [];
          for (var i = 0; i < arguments.length; i++) {
            ret.push(arguments[i]);
          }
        }
      })
      .addErrback(function (arg) {
        had_error = true;
        ret = arg;
      })
      .block();

  if (had_error) throw ret;
  return ret;
};

})(); // end anonymous namespace
