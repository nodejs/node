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
const errors = require('internal/errors');
const { createHook } = require('async_hooks');

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

const pairing = new Map();
const asyncHook = createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (process.domain !== null && process.domain !== undefined) {
      // if this operation is created while in a domain, let's mark it
      pairing.set(asyncId, process.domain);
      resource.domain = process.domain;
      if (resource.promise !== undefined &&
          resource.promise instanceof Promise) {
        // resource.promise instanceof Promise make sure that the
        // promise comes from the same context
        // see https://github.com/nodejs/node/issues/15673
        resource.promise.domain = process.domain;
      }
    }
  },
  before(asyncId) {
    const current = pairing.get(asyncId);
    if (current !== undefined) { // enter domain for this cb
      current.enter();
    }
  },
  after(asyncId) {
    const current = pairing.get(asyncId);
    if (current !== undefined) { // exit domain for this cb
      current.exit();
    }
  },
  destroy(asyncId) {
    pairing.delete(asyncId); // cleaning up
  }
});

// When domains are in use, they claim full ownership of the
// uncaught exception capture callback.
if (process.hasUncaughtExceptionCaptureCallback()) {
  throw new errors.Error('ERR_DOMAIN_CALLBACK_NOT_AVAILABLE');
}

// Get the stack trace at the point where `domain` was required.
const domainRequireStack = new Error('require(`domain`) at this point').stack;

const { setUncaughtExceptionCaptureCallback } = process;
process.setUncaughtExceptionCaptureCallback = function(fn) {
  const err =
      new errors.Error('ERR_DOMAIN_CANNOT_SET_UNCAUGHT_EXCEPTION_CAPTURE');
  err.stack = err.stack + '\n' + '-'.repeat(40) + '\n' + domainRequireStack;
  throw err;
};

function topLevelDomainCallback(cb, ...args) {
  const domain = this.domain;
  if (domain)
    domain.enter();
  const ret = Reflect.apply(cb, this, args);
  if (domain)
    domain.exit();
  return ret;
}

// It's possible to enter one domain while already inside
// another one. The stack is each entered domain.
const stack = [];
exports._stack = stack;
internalBinding('domain').enable(topLevelDomainCallback);

function updateExceptionCapture() {
  if (stack.every((domain) => domain.listenerCount('error') === 0)) {
    setUncaughtExceptionCaptureCallback(null);
  } else {
    setUncaughtExceptionCaptureCallback(null);
    setUncaughtExceptionCaptureCallback((er) => {
      return process.domain._errorHandler(er);
    });
  }
}


process.on('newListener', (name, listener) => {
  if (name === 'uncaughtException' &&
      listener !== domainUncaughtExceptionClear) {
    // Make sure the first listener for `uncaughtException` always clears
    // the domain stack.
    process.removeListener(name, domainUncaughtExceptionClear);
    process.prependListener(name, domainUncaughtExceptionClear);
  }
});

process.on('removeListener', (name, listener) => {
  if (name === 'uncaughtException' &&
      listener !== domainUncaughtExceptionClear) {
    // If the domain listener would be the only remaining one, remove it.
    const listeners = process.listeners('uncaughtException');
    if (listeners.length === 1 && listeners[0] === domainUncaughtExceptionClear)
      process.removeListener(name, domainUncaughtExceptionClear);
  }
});

function domainUncaughtExceptionClear() {
  stack.length = 0;
  exports.active = process.domain = null;
  updateExceptionCapture();
}


class Domain extends EventEmitter {
  constructor() {
    super();

    this.members = [];
    asyncHook.enable();

    this.on('removeListener', updateExceptionCapture);
    this.on('newListener', updateExceptionCapture);
  }
}

exports.Domain = Domain;

exports.create = exports.createDomain = function() {
  return new Domain();
};

// the active domain is always the one that we're currently in.
exports.active = null;
Domain.prototype.members = undefined;

// Called by process._fatalException in case an error was thrown.
Domain.prototype._errorHandler = function _errorHandler(er) {
  var caught = false;

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
      // Clear the uncaughtExceptionCaptureCallback so that we know that, even
      // if technically the top-level domain is still active, it would
      // be ok to abort on an uncaught exception at this point
      setUncaughtExceptionCaptureCallback(null);
      try {
        caught = this.emit('error', er);
      } finally {
        updateExceptionCapture();
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
      updateExceptionCapture();
      if (stack.length) {
        exports.active = process.domain = stack[stack.length - 1];
        caught = process.domain._errorHandler(er2);
      } else {
        // Pass on to the next exception handler.
        throw er2;
      }
    }
  }

  // Exit all domains on the stack.  Uncaught exceptions end the
  // current tick and no domains should be left on the stack
  // between ticks.
  domainUncaughtExceptionClear();

  return caught;
};


Domain.prototype.enter = function() {
  // note that this might be a no-op, but we still need
  // to push it onto the stack so that we can pop it later.
  exports.active = process.domain = this;
  stack.push(this);
  updateExceptionCapture();
};


Domain.prototype.exit = function() {
  // don't do anything if this domain is not on the stack.
  var index = stack.lastIndexOf(this);
  if (index === -1) return;

  // exit all domains until this one.
  stack.splice(index);

  exports.active = stack[stack.length - 1];
  process.domain = exports.active;
  updateExceptionCapture();
};


// note: this works for timers as well.
Domain.prototype.add = function(ee) {
  // If the domain is already added, then nothing left to do.
  if (ee.domain === this)
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

// Override EventEmitter methods to make it domain-aware.
EventEmitter.usingDomains = true;

const eventInit = EventEmitter.init;
EventEmitter.init = function() {
  this.domain = null;
  if (exports.active && !(this instanceof exports.Domain)) {
    this.domain = exports.active;
  }

  return eventInit.call(this);
};

const eventEmit = EventEmitter.prototype.emit;
EventEmitter.prototype.emit = function emit(...args) {
  const domain = this.domain;

  const type = args[0];
  const shouldEmitError = type === 'error' &&
                          this.listenerCount(type) > 0;

  // Just call original `emit` if current EE instance has `error`
  // handler, there's no active domain or this is process
  if (shouldEmitError || domain === null || domain === undefined ||
      this === process) {
    return Reflect.apply(eventEmit, this, args);
  }

  if (type === 'error') {
    const er = args.length > 1 && args[1] ?
      args[1] : new errors.Error('ERR_UNHANDLED_ERROR');

    if (typeof er === 'object') {
      er.domainEmitter = this;
      er.domain = domain;
      er.domainThrown = false;
    }

    domain.emit('error', er);
    return false;
  }

  domain.enter();
  const ret = Reflect.apply(eventEmit, this, args);
  domain.exit();

  return ret;
};
