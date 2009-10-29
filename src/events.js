(function () {

// process.EventEmitter is defined in src/events.cc
// process.EventEmitter.prototype.emit() is also defined there.
process.EventEmitter.prototype.addListener = function (type, listener) {
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

process.EventEmitter.prototype.listeners = function (type) {
  if (!this._events) this._events = {};
  if (!this._events.hasOwnProperty(type)) this._events[type] = [];
  return this._events[type];
};

process.Promise.prototype.timeout = function(timeout) {
  if (timeout === undefined) {
    return this._timeoutDuration;
  }

  this._timeoutDuration = timeout;
  if (this._timer) {
    clearTimeout(this._timer);
  }

  var promiseComplete = false;
  var onComplete = function() {
    promiseComplete = true;
  };

  this
    .addCallback(onComplete)
    .addCancelback(onComplete)
    .addErrback(onComplete);

  var self = this
  this._timer = setTimeout(function() {
    if (promiseComplete) {
      return;
    }

    self.emitError(new Error('timeout'));
  }, this._timeoutDuration);

  return this;
};

process.Promise.prototype.cancel = function() {
  this._events['success'] = [];
  this._events['error'] = [];

  this.emitCancel();
};

process.Promise.prototype.emitCancel = function() {
  var args = Array.prototype.slice.call(arguments);
  args.unshift('cancel');

  this.emit.apply(this, args);
};

process.Promise.prototype.addCallback = function (listener) {
  this.addListener("success", listener);
  return this;
};

process.Promise.prototype.addErrback = function (listener) {
  this.addListener("error", listener);
  return this;
};

process.Promise.prototype.addCancelback = function (listener) {
  this.addListener("cancel", listener);
  return this;
};

process.Promise.prototype.wait = function () {
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

  if (had_error) {
    if (ret) {
      throw ret;
    } else {
      throw new Error("Promise completed with error (No arguments given.)");
    }
  }
  return ret;
};

})(); // end anonymous namespace
