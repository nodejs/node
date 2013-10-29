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

var util = require('util');
var EventEmitter = require('events');
var inherits = util.inherits;

// communicate with events module, but don't require that
// module to have to load this one, since this module has
// a few side effects.
EventEmitter.usingDomains = true;

exports.Domain = Domain;

exports.create = exports.createDomain = function() {
  return new Domain();
};

// it's possible to enter one domain while already inside
// another one.  the stack is each entered domain.
var stack = [];
exports._stack = stack;
// the active domain is always the one that we're currently in.
exports.active = null;


function noop() { }


var listenerObj = {
  error: function errorHandler(domain, er) {
    var caught = false;
    // ignore errors on disposed domains.
    //
    // XXX This is a bit stupid.  We should probably get rid of
    // domain.dispose() altogether.  It's almost always a terrible
    // idea.  --isaacs
    if (domain._disposed)
      return true;

    er.domain = domain;
    er.domainThrown = true;
    // wrap this in a try/catch so we don't get infinite throwing
    try {
      // One of three things will happen here.
      //
      // 1. There is a handler, caught = true
      // 2. There is no handler, caught = false
      // 3. It throws, caught = false
      //
      // If caught is false after this, then there's no need to exit()
      // the domain, because we're going to crash the process anyway.
      caught = domain.emit('error', er);

      if (stack.length === 0)
        process.removeAsyncListener(domain._listener);

      // Exit all domains on the stack.  Uncaught exceptions end the
      // current tick and no domains should be left on the stack
      // between ticks.
      stack.length = 0;
      exports.active = process.domain = null;
    } catch (er2) {
      // The domain error handler threw!  oh no!
      // See if another domain can catch THIS error,
      // or else crash on the original one.
      // If the user already exited it, then don't double-exit.
      if (domain === exports.active) {
        stack.pop();
      }
      if (stack.length) {
        exports.active = process.domain = stack[stack.length - 1];
        caught = process._fatalException(er2);
      } else {
        caught = false;
      }
      return caught;
    }
    return caught;
  }
};


inherits(Domain, EventEmitter);

function Domain() {
  EventEmitter.call(this);
  this.members = [];
  this._listener = process.createAsyncListener(noop, listenerObj, this);
}

Domain.prototype.members = undefined;
Domain.prototype._disposed = undefined;
Domain.prototype._listener = undefined;


Domain.prototype.enter = function() {
  if (this._disposed) return;

  // note that this might be a no-op, but we still need
  // to push it onto the stack so that we can pop it later.
  exports.active = process.domain = this;
  stack.push(this);

  process.addAsyncListener(this._listener);
};


Domain.prototype.exit = function() {
  if (this._disposed) return;

  process.removeAsyncListener(this._listener);

  // exit all domains until this one.
  var index = stack.lastIndexOf(this);
  if (index !== -1)
    stack.splice(index + 1);
  else
    stack.length = 0;

  exports.active = stack[stack.length - 1];
  process.domain = exports.active;
};


// note: this works for timers as well.
Domain.prototype.add = function(ee) {
  // If the domain is disposed or already added, then nothing left to do.
  if (this._disposed || ee.domain === this)
    return;

  // has a domain already - remove it first.
  if (ee.domain)
    ee.domain.remove(ee);

  // check for circular Domain->Domain links.
  // This causes bad insanity!
  //
  // For example:
  // var d = domain.create();
  // var e = domain.create();
  // d.add(e);
  // e.add(d);
  // e.emit('error', er); // RangeError, stack overflow!
  if (this.domain && (ee instanceof Domain)) {
    for (var d = this.domain; d; d = d.domain) {
      if (ee === d) return;
    }
  }

  ee.domain = this;
  this.members.push(ee);

  // Adding the domain._listener to the Wrap associated with the event
  // emitter instance will be done automatically either on class
  // instantiation or manually, like in cases of net listen().
  // The reason it cannot be done here is because in specific cases the
  // _handle is not created on EE instantiation, so there's no place to
  // add the listener.
};


Domain.prototype.remove = function(ee) {
  ee.domain = null;
  var index = this.members.indexOf(ee);
  if (index !== -1)
    this.members.splice(index, 1);

  // First check if the ee is a handle itself.
  if (ee.removeAsyncListener)
    ee.removeAsyncListener(this._listener);

  // Manually remove the asyncListener from the handle, if possible.
  if (ee._handle && ee._handle.removeAsyncListener)
    ee._handle.removeAsyncListener(this._listener);

  // TODO(trevnorris): Are there cases where the handle doesn't live on
  // the ee or the _handle.

  // TODO(trevnorris): For debugging that we've missed adding AsyncWrap's
  // methods to a handle somewhere on the native side.
  if (ee._handle && !ee._handle.removeAsyncListener) {
    process._rawDebug('Wrap handle is missing AsyncWrap methods');
    process.abort();
  }
};


Domain.prototype.run = function(fn) {
  if (this._disposed)
    return;
  this.enter();
  var ret = fn.call(this);
  this.exit();
  return ret;
};


function intercepted(_this, self, cb, fnargs) {
  if (self._disposed)
    return;

  if (fnargs[0] && fnargs[0] instanceof Error) {
    var er = fnargs[0];
    util._extend(er, {
      domainBound: cb,
      domainThrown: false,
      domain: self
    });
    self.emit('error', er);
    return;
  }

  var len = fnargs.length;
  var args = [];
  var i, ret;

  self.enter();
  if (fnargs.length > 1) {
    for (i = 1; i < fnargs.length; i++)
      args.push(fnargs[i]);
    ret = cb.apply(_this, args);
  } else {
    ret = cb.call(_this);
  }
  self.exit();

  return ret;
}


Domain.prototype.intercept = function(cb) {
  var self = this;

  function runIntercepted() {
    return intercepted(this, self, cb, arguments);
  }

  return runIntercepted;
};


function bound(_this, self, cb, fnargs) {
  if (self._disposed)
    return;

  var len = fnargs.length;
  var args = [];
  var i, ret;

  self.enter();
  if (fnargs.length > 0)
    ret = cb.apply(_this, fnargs);
  else
    ret = cb.call(_this);
  self.exit();

  return ret;
}


Domain.prototype.bind = function(cb) {
  var self = this;

  function runBound() {
    return bound(this, self, cb, arguments);
  }

  runBound.domain = this;

  return runBound;
};


Domain.prototype.dispose = util.deprecate(function() {
  if (this._disposed) return;

  // if we're the active domain, then get out now.
  this.exit();

  // remove from parent domain, if there is one.
  if (this.domain) this.domain.remove(this);

  // kill the references so that they can be properly gc'ed.
  this.members.length = 0;

  // mark this domain as 'no longer relevant'
  // so that it can't be entered or activated.
  this._disposed = true;
});
