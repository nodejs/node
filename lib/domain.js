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

'use strict';

// WARNING: THIS MODULE IS PENDING DEPRECATION.
//
// No new pull requests targeting this module will be accepted
// unless they address existing, critical bugs.

const util = require('util');
const EventEmitter = require('events');
const inherits = util.inherits;

// communicate with events module, but don't require that
// module to have to load this one, since this module has
// a few side effects.
EventEmitter.usingDomains = true;

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

// It's possible to enter one domain while already inside
// another one. The stack is each entered domain.
const stack = [];
exports._stack = stack;

// let the process know we're using domains
const _domain_flag = process._setupDomainUse(_domain, stack);

exports.Domain = Domain;

exports.create = exports.createDomain = function() {
  return new Domain();
};

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
Domain.prototype._errorHandler = function _errorHandler(er) {
  var caught = false;

  // ignore errors on disposed domains.
  //
  // XXX This is a bit stupid.  We should probably get rid of
  // domain.dispose() altogether.  It's almost always a terrible
  // idea.  --isaacs
  if (this._disposed)
    return true;

  if (!util.isPrimitive(er)) {
    er.domain = this;
    er.domainThrown = true;
  }

  // The top-level domain-handler is handled separately.
  //
  // The reason is that if V8 was passed a command line option
  // asking it to abort on an uncaught exception (currently
  // "--abort-on-uncaught-exception"), we want an uncaught exception
  // in the top-level domain error handler to make the
  // process abort. Using try/catch here would always make V8 think
  // that these exceptions are caught, and thus would prevent it from
  // aborting in these cases.
  if (stack.length === 1) {
    // If there's no error handler, do not emit an 'error' event
    // as this would throw an error, make the process exit, and thus
    // prevent the process 'uncaughtException' event from being emitted
    // if a listener is set.
    if (EventEmitter.listenerCount(this, 'error') > 0) {
      try {
        // Set the _emittingTopLevelDomainError so that we know that, even
        // if technically the top-level domain is still active, it would
        // be ok to abort on an uncaught exception at this point
        process._emittingTopLevelDomainError = true;
        caught = this.emit('error', er);
      } finally {
        process._emittingTopLevelDomainError = false;
      }
    }
  } else {
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
    }
  }

  // Exit all domains on the stack.  Uncaught exceptions end the
  // current tick and no domains should be left on the stack
  // between ticks.
  stack.length = 0;
  exports.active = process.domain = null;

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
  // skip disposed domains, as usual, but also don't do anything if this
  // domain is not on the stack.
  var index = stack.lastIndexOf(this);
  if (this._disposed || index === -1) return;

  // exit all domains until this one.
  stack.splice(index);
  _domain_flag[0] = stack.length;

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
};


Domain.prototype.remove = function(ee) {
  ee.domain = null;
  var index = this.members.indexOf(ee);
  if (index !== -1)
    this.members.splice(index, 1);
};


Domain.prototype.run = function(fn) {
  if (this._disposed)
    return;

  var ret;

  this.enter();
  if (arguments.length >= 2) {
    var len = arguments.length;
    var args = new Array(len - 1);

    for (var i = 1; i < len; i++)
      args[i - 1] = arguments[i];

    ret = fn.apply(this, args);
  } else {
    ret = fn.call(this);
  }
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

  var ret;

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
}, 'Domain.dispose is deprecated. Recover from failed I/O actions explicitly ' +
    'via error event handlers set on the domain instead.', 'DEP0012');
