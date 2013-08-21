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
var events = require('events');
var EventEmitter = events.EventEmitter;
var inherits = util.inherits;

// methods that are called when trying to shut down explicitly bound EEs
var endMethods = ['end', 'abort', 'destroy', 'destroySoon'];

// communicate with events module, but don't require that
// module to have to load this one, since this module has
// a few side effects.
events.usingDomains = true;

// overwrite process.domain with a getter/setter that will allow for more
// effective optimizations
var _domain = [null];
Object.defineProperty(process, 'domain', {
  enumerable: true,
  get: function() {
    return _domain[0];
  },
  set: function(arg) {
    return _domain[0] = arg;
  }
});

// objects with external array data are excellent ways to communicate state
// between js and c++ w/o much overhead
var _domain_flag = {};

// let the process know we're using domains
process._setupDomainUse(_domain, _domain_flag);

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


inherits(Domain, EventEmitter);

function Domain() {
  EventEmitter.call(this);

  this.members = [];
}

Domain.prototype.members = undefined;
Domain.prototype._disposed = undefined;

// Called by process._fatalException in case an error was thrown.
Domain.prototype._errorHandler = function errorHandler(er) {
  var caught = false;
  // ignore errors on disposed domains.
  //
  // XXX This is a bit stupid.  We should probably get rid of
  // domain.dispose() altogether.  It's almost always a terrible
  // idea.  --isaacs
  if (this._disposed)
    return true;

  er.domain = this;
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
    caught = this.emit('error', er);

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
    if (this === exports.active) {
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
};

Domain.prototype.enter = function() {
  if (this._disposed) return;

  // note that this might be a no-op, but we still need
  // to push it onto the stack so that we can pop it later.
  exports.active = process.domain = this;
  stack.push(this);
  _domain_flag[0] = stack.length;
};

Domain.prototype.exit = function() {
  if (this._disposed) return;

  // exit all domains until this one.
  var d;
  do {
    d = stack.pop();
  } while (d && d !== this);
  _domain_flag[0] = stack.length;

  exports.active = stack[stack.length - 1];
  process.domain = exports.active;
};

// note: this works for timers as well.
Domain.prototype.add = function(ee) {
  // disposed domains can't be used for new things.
  if (this._disposed) return;

  // already added to this domain.
  if (ee.domain === this) return;

  // has a domain already - remove it first.
  if (ee.domain) {
    ee.domain.remove(ee);
  }

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
};

Domain.prototype.remove = function(ee) {
  ee.domain = null;
  var index = this.members.indexOf(ee);
  if (index !== -1) {
    this.members.splice(index, 1);
  }
};

Domain.prototype.run = function(fn) {
  return this.bind(fn)();
};

Domain.prototype.intercept = function(cb) {
  return this.bind(cb, true);
};

Domain.prototype.bind = function(cb, interceptError) {
  // if cb throws, catch it here.
  var self = this;
  var b = function() {
    // disposing turns functions into no-ops
    if (self._disposed) return;

    if (this instanceof Domain) {
      return cb.apply(this, arguments);
    }

    // only intercept first-arg errors if explicitly requested.
    if (interceptError && arguments[0] &&
        (arguments[0] instanceof Error)) {
      var er = arguments[0];
      util._extend(er, {
        domainBound: cb,
        domainThrown: false,
        domain: self
      });
      self.emit('error', er);
      return;
    }

    // remove first-arg if intercept as assumed to be the error-arg
    if (interceptError) {
      var len = arguments.length;
      var args;
      switch (len) {
        case 0:
        case 1:
          // no args that we care about.
          args = [];
          break;
        case 2:
          // optimization for most common case: cb(er, data)
          args = [arguments[1]];
          break;
        default:
          // slower for less common case: cb(er, foo, bar, baz, ...)
          args = new Array(len - 1);
          for (var i = 1; i < len; i++) {
            args[i - 1] = arguments[i];
          }
          break;
      }
      self.enter();
      var ret = cb.apply(this, args);
      self.exit();
      return ret;
    }

    self.enter();
    var ret = cb.apply(this, arguments);
    self.exit();
    return ret;
  };
  b.domain = this;
  return b;
};

Domain.prototype.dispose = function() {
  if (this._disposed) return;

  // if we're the active domain, then get out now.
  this.exit();

  this.emit('dispose');

  // remove error handlers.
  this.removeAllListeners();
  this.on('error', function() {});

  // try to kill all the members.
  // XXX There should be more consistent ways
  // to shut down things!
  this.members.forEach(function(m) {
    // if it's a timeout or interval, cancel it.
    clearTimeout(m);

    // drop all event listeners.
    if (m instanceof EventEmitter) {
      m.removeAllListeners();
      // swallow errors
      m.on('error', function() {});
    }

    // Be careful!
    // By definition, we're likely in error-ridden territory here,
    // so it's quite possible that calling some of these methods
    // might cause additional exceptions to be thrown.
    endMethods.forEach(function(method) {
      if (util.isFunction(m[method])) {
        try {
          m[method]();
        } catch (er) {}
      }
    });

  });

  // remove from parent domain, if there is one.
  if (this.domain) this.domain.remove(this);

  // kill the references so that they can be properly gc'ed.
  this.members.length = 0;

  // finally, mark this domain as 'no longer relevant'
  // so that it can't be entered or activated.
  this._disposed = true;
};
