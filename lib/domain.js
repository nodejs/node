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

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeLastIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  Error,
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ReflectApply,
  SafeSet,
  StringPrototypeRepeat,
} = primordials;

const EventEmitter = require('events');
const {
  ERR_DOMAIN_CALLBACK_NOT_AVAILABLE,
  ERR_DOMAIN_CANNOT_SET_UNCAUGHT_EXCEPTION_CAPTURE,
  ERR_UNHANDLED_ERROR,
} = require('internal/errors').codes;

// Use AsyncLocalStorage for domain context propagation
// Lazy initialization - ALS is created on first domain use
let domainStorage;

function initDomainStorage() {
  if (domainStorage === undefined) {
    const { AsyncLocalStorage } = require('async_hooks');

    // Single AsyncLocalStorage instance for all domain context
    // Store structure: { domain: Domain | null, stack: Domain[] }
    domainStorage = new AsyncLocalStorage();
  }
  return domainStorage;
}

// Helper functions to get/set domain store
function getDomainStore() {
  return initDomainStorage().getStore();
}

function setDomainStore(store) {
  initDomainStorage().enterWith(store);
}

// Overwrite process.domain with a getter/setter backed by native context
ObjectDefineProperty(process, 'domain', {
  __proto__: null,
  enumerable: true,
  get: function() {
    const store = getDomainStore();
    // Return undefined when no domain is active (store doesn't exist OR domain is null)
    // This makes the API consistent: undefined = no domain, Domain object = domain active
    const domain = store?.domain;
    return domain === null ? undefined : domain;
  },
  set: function(arg) {
    const store = getDomainStore();
    if (store) {
      // Create new store to avoid affecting other async contexts
      setDomainStore({ domain: arg, stack: store.stack });
    }
    // If no store exists, setting is a no-op (will be set when domain is entered)
    return arg;
  },
});

// When domains are in use, they claim full ownership of the
// uncaught exception capture callback.
if (process.hasUncaughtExceptionCaptureCallback()) {
  throw new ERR_DOMAIN_CALLBACK_NOT_AVAILABLE();
}

// Get the stack trace at the point where `domain` was required.
// eslint-disable-next-line no-restricted-syntax
const domainRequireStack = new Error('require(`domain`) at this point').stack;

const { setUncaughtExceptionCaptureCallback } = process;
process.setUncaughtExceptionCaptureCallback = function(fn) {
  const err = new ERR_DOMAIN_CANNOT_SET_UNCAUGHT_EXCEPTION_CAPTURE();
  err.stack += `\n${StringPrototypeRepeat('-', 40)}\n${domainRequireStack}`;
  throw err;
};


// Helper to get the current stack from native context
function getStack() {
  const store = getDomainStore();
  return store?.stack ?? [];
}

// Helper to set the stack in native context
function setStack(newStack) {
  const store = getDomainStore();
  if (store) {
    // Create new store to avoid affecting other async contexts
    setDomainStore({ domain: store.domain, stack: newStack });
  }
}

// It's possible to enter one domain while already inside
// another one. The stack is each entered domain.
// For backward compatibility, exports._stack is a getter/setter
ObjectDefineProperty(exports, '_stack', {
  __proto__: null,
  enumerable: true,
  get: getStack,
  set: setStack,
});

// Module-level tracking of the current domain and stack for exception handling.
// This is needed because when exceptions propagate out of domainStorage.run(),
// the ALS context is restored before the exception capture callback runs.
// For async callbacks, ALS context is still available, but for synchronous
// exceptions we need this fallback.
let currentDomain = null;
let currentStack = null;  // The stack at the time of the current sync operation

// Track all domains that have error listeners. This is needed because async
// callbacks may fire with a domain context even when the domain is not on the
// current stack. We install the exception handler when ANY domain has listeners.
const domainsWithListeners = new SafeSet();

// The domain exception handler - only installed when a domain with error listeners
// exists. This allows uncaughtException event to fire when no handler.
function domainExceptionHandler(er) {
  // For sync exceptions (from run/bound/intercepted), currentDomain is set to the
  // domain that was active when the exception was thrown. Check it first because
  // by the time we get here, the ALS context may have already been restored to the
  // outer domain.
  // For async callbacks, currentDomain is null and process.domain reads from ALS.
  const domain = currentDomain || process.domain;

  // For sync exceptions, we need to restore the ALS context before calling _errorHandler
  // because ALS unwinds during exception propagation. This ensures exit() works correctly.
  if (currentDomain !== null && currentStack !== null) {
    // Restore the ALS context and exports.active to match the state at the time of the throw
    setDomainStore({ domain: currentDomain, stack: currentStack });
    exports.active = currentDomain;
  }

  // First check the current domain
  if (domain && domain.listenerCount('error') > 0) {
    return domain._errorHandler(er);
  }

  // For SYNC exceptions only (currentDomain is set), check parent domains on the stack.
  // This handles the case where d2 (no handler) throws synchronously inside d (has handler).
  // For ASYNC exceptions (currentDomain is null), only the domain from ALS matters -
  // the exception should NOT bubble up to parent domains that were on the stack when
  // the async callback was scheduled.
  if (currentDomain !== null && currentStack !== null) {
    for (let i = currentStack.length - 1; i >= 0; i--) {
      const parentDomain = currentStack[i];
      if (parentDomain !== domain && parentDomain.listenerCount('error') > 0) {
        return parentDomain._errorHandler(er);
      }
    }
  }

  // No domain with error handler found - let the exception propagate.
  // Since setUncaughtExceptionCaptureCallback ignores our return value,
  // we must emit 'uncaughtException' manually to give other handlers a chance.
  // We temporarily uninstall ourselves to avoid infinite recursion if someone
  // throws from their uncaughtException handler.
  uninstallExceptionHandler();

  // Emit uncaughtException to give process.on('uncaughtException') handlers a chance
  const handled = process.emit('uncaughtException', er, 'uncaughtException');

  if (handled) {
    // Reinstall ourselves for future exceptions
    installExceptionHandler();
  } else {
    // No handler caught the exception - we need to exit.
    // Throw it to let the C++ side handle the exit properly (respects --abort-on-uncaught).
    // Don't reinstall the callback - we want the throw to propagate directly to C++.
    throw er;
  }
}

// Track if exception handler is currently installed
let exceptionHandlerInstalled = false;

function installExceptionHandler() {
  if (!exceptionHandlerInstalled) {
    setUncaughtExceptionCaptureCallback(domainExceptionHandler);
    exceptionHandlerInstalled = true;
  }
}

function uninstallExceptionHandler() {
  if (exceptionHandlerInstalled) {
    setUncaughtExceptionCaptureCallback(null);
    exceptionHandlerInstalled = false;
  }
}

function updateExceptionCapture() {
  // Install exception handler if ANY domain has error listeners
  if (domainsWithListeners.size > 0) {
    installExceptionHandler();
  } else {
    uninstallExceptionHandler();
  }
}

// Called when an 'error' listener is being added
// 'newListener' is called BEFORE listener is added, so count is still 0
function domainNewListener(type) {
  if (type === 'error') {
    domainsWithListeners.add(this);
    updateExceptionCapture();
  }
}

// Called when an 'error' listener is being removed
// 'removeListener' is called AFTER listener is removed, so count reflects new state
function domainRemoveListener(type) {
  if (type === 'error' && this.listenerCount('error') === 0) {
    domainsWithListeners.delete(this);
    updateExceptionCapture();
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
  // Clear the stack and active domain in the CURRENT context only
  // Don't mutate the store in-place as it would affect other async contexts
  // that share the same store reference
  setDomainStore({ domain: null, stack: [] });
  exports.active = null;
  currentDomain = null;
  currentStack = null;
  updateExceptionCapture();
}


class Domain extends EventEmitter {
  constructor() {
    super();

    this.members = [];

    this.on('newListener', domainNewListener);
    this.on('removeListener', domainRemoveListener);
  }
}

exports.Domain = Domain;

exports.create = exports.createDomain = function createDomain() {
  return new Domain();
};

// The active domain is always the one that we're currently in.
exports.active = null;
Domain.prototype.members = undefined;

// Called by process._fatalException in case an error was thrown.
Domain.prototype._errorHandler = function(er) {
  let caught = false;

  if ((typeof er === 'object' && er !== null) || typeof er === 'function') {
    ObjectDefineProperty(er, 'domain', {
      __proto__: null,
      configurable: true,
      enumerable: false,
      value: this,
      writable: true,
    });
    er.domainThrown = true;
  }
  // Pop all adjacent duplicates of the currently active domain from the stack.
  // This is done to prevent a domain's error handler to run within the context
  // of itself, and re-entering itself recursively handler as a result of an
  // exception thrown in its context.
  while (exports.active === this) {
    this.exit();
  }

  const stack = getStack();

  // The top-level domain-handler is handled separately.
  //
  // The reason is that if V8 was passed a command line option
  // asking it to abort on an uncaught exception (currently
  // "--abort-on-uncaught-exception"), we want an uncaught exception
  // in the top-level domain error handler to make the
  // process abort. Using try/catch here would always make V8 think
  // that these exceptions are caught, and thus would prevent it from
  // aborting in these cases.
  if (stack.length === 0) {
    // If there's no error handler, do not emit an 'error' event
    // as this would throw an error, make the process exit, and thus
    // prevent the process 'uncaughtException' event from being emitted
    // if a listener is set.
    if (this.listenerCount('error') > 0) {
      // Clear the uncaughtExceptionCaptureCallback so that we know that, since
      // the top-level domain is not active anymore, it would be ok to abort on
      // an uncaught exception at this point
      uninstallExceptionHandler();
      try {
        caught = this.emit('error', er);
      } finally {
        installExceptionHandler();
      }
    }
  } else {
    // Wrap this in a try/catch so we don't get infinite throwing
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
      updateExceptionCapture();
      const currentStack = getStack();
      if (currentStack.length) {
        const parentDomain = currentStack[currentStack.length - 1];
        const store = getDomainStore();
        if (store) {
          // Create new store to avoid affecting other async contexts
          setDomainStore({ domain: parentDomain, stack: store.stack });
        }
        exports.active = parentDomain;
        caught = parentDomain._errorHandler(er2);
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
  // Note that this might be a no-op, but we still need
  // to push it onto the stack so that we can pop it later.
  const currentStore = getDomainStore() || { domain: null, stack: [] };
  const newStack = ArrayPrototypeSlice(currentStore.stack);
  ArrayPrototypePush(newStack, this);
  setDomainStore({ domain: this, stack: newStack });
  exports.active = this;
  currentDomain = this;
  currentStack = newStack;
  updateExceptionCapture();
};


Domain.prototype.exit = function() {
  // Don't do anything if this domain is not on the stack.
  const store = getDomainStore();
  if (!store) return;

  const stack = store.stack;
  const index = ArrayPrototypeLastIndexOf(stack, this);
  if (index === -1) return;

  // Exit all domains until this one.
  const newStack = ArrayPrototypeSlice(stack, 0, index);
  const parentDomain = newStack.length === 0 ? null : newStack[newStack.length - 1];
  setDomainStore({ domain: parentDomain, stack: newStack });
  exports.active = parentDomain;
  currentDomain = parentDomain;
  currentStack = newStack.length === 0 ? null : newStack;
  updateExceptionCapture();
};


// note: this works for timers as well.
Domain.prototype.add = function(ee) {
  // If the domain is already added, then nothing left to do.
  if (ee.domain === this)
    return;

  // Has a domain already - remove it first.
  if (ee.domain)
    ee.domain.remove(ee);

  // Check for circular Domain->Domain links.
  // They cause big issues.
  //
  // For example:
  // var d = domain.create();
  // var e = domain.create();
  // d.add(e);
  // e.add(d);
  // e.emit('error', er); // RangeError, stack overflow!
  if (this.domain && (ee instanceof Domain)) {
    for (let d = this.domain; d; d = d.domain) {
      if (ee === d) return;
    }
  }

  ObjectDefineProperty(ee, 'domain', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: this,
    writable: true,
  });
  ArrayPrototypePush(this.members, ee);
};


Domain.prototype.remove = function(ee) {
  ee.domain = null;
  const index = ArrayPrototypeIndexOf(this.members, ee);
  if (index !== -1)
    ArrayPrototypeSplice(this.members, index, 1);
};


Domain.prototype.run = function(fn) {
  const currentStore = getDomainStore() || { domain: null, stack: [] };
  const newStack = ArrayPrototypeSlice(currentStore.stack);
  ArrayPrototypePush(newStack, this);

  const args = ArrayPrototypeSlice(arguments, 1);
  const previousDomain = currentDomain;
  const previousStack = currentStack;

  // Set currentDomain and currentStack before running so exception handler can see them
  currentDomain = this;
  currentStack = newStack;
  updateExceptionCapture();

  // Use enterWith instead of run() so the context is NOT automatically restored on exception.
  // This matches the original domain.run() behavior where exit() only runs on success.
  setDomainStore({ domain: this, stack: newStack });
  exports.active = this;

  const result = ReflectApply(fn, this, args);

  // On success, restore context (if exception thrown, context stays for catch blocks)
  setDomainStore(currentStore);
  exports.active = currentStore.domain;
  currentDomain = previousDomain;
  currentStack = previousStack;
  updateExceptionCapture();

  return result;
};


function intercepted(_this, self, cb, fnargs) {
  if (fnargs[0] && fnargs[0] instanceof Error) {
    const er = fnargs[0];
    er.domainBound = cb;
    er.domainThrown = false;
    ObjectDefineProperty(er, 'domain', {
      __proto__: null,
      configurable: true,
      enumerable: false,
      value: self,
      writable: true,
    });
    self.emit('error', er);
    return;
  }

  const currentStore = getDomainStore() || { domain: null, stack: [] };
  const newStack = ArrayPrototypeSlice(currentStore.stack);
  ArrayPrototypePush(newStack, self);
  const args = ArrayPrototypeSlice(fnargs, 1);
  const previousDomain = currentDomain;
  const previousStack = currentStack;

  currentDomain = self;
  currentStack = newStack;
  updateExceptionCapture();

  // Use enterWith instead of run() so the context is NOT automatically restored on exception.
  setDomainStore({ domain: self, stack: newStack });
  exports.active = self;

  const result = ReflectApply(cb, _this, args);

  // On success, restore context
  setDomainStore(currentStore);
  exports.active = currentStore.domain;
  currentDomain = previousDomain;
  currentStack = previousStack;
  updateExceptionCapture();

  return result;
}


Domain.prototype.intercept = function(cb) {
  const self = this;

  function runIntercepted() {
    return intercepted(this, self, cb, arguments);
  }

  return runIntercepted;
};


function bound(_this, self, cb, fnargs) {
  const currentStore = getDomainStore() || { domain: null, stack: [] };
  const newStack = ArrayPrototypeSlice(currentStore.stack);
  ArrayPrototypePush(newStack, self);
  const previousDomain = currentDomain;
  const previousStack = currentStack;

  currentDomain = self;
  currentStack = newStack;
  updateExceptionCapture();

  // Use enterWith instead of run() so the context is NOT automatically restored on exception.
  setDomainStore({ domain: self, stack: newStack });
  exports.active = self;

  const result = ReflectApply(cb, _this, fnargs);

  // On success, restore context
  setDomainStore(currentStore);
  exports.active = currentStore.domain;
  currentDomain = previousDomain;
  currentStack = previousStack;
  updateExceptionCapture();

  return result;
}


Domain.prototype.bind = function(cb) {
  const self = this;

  function runBound() {
    return bound(this, self, cb, arguments);
  }

  ObjectDefineProperty(runBound, 'domain', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: this,
    writable: true,
  });

  return runBound;
};

// Override EventEmitter methods to make it domain-aware.
EventEmitter.usingDomains = true;

const eventInit = EventEmitter.init;
EventEmitter.init = function(opts) {
  ObjectDefineProperty(this, 'domain', {
    __proto__: null,
    configurable: true,
    enumerable: false,
    value: null,
    writable: true,
  });
  if (exports.active && !(this instanceof exports.Domain)) {
    this.domain = exports.active;
  }

  return FunctionPrototypeCall(eventInit, this, opts);
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
    return ReflectApply(eventEmit, this, args);
  }

  if (type === 'error') {
    const er = args.length > 1 && args[1] ?
      args[1] : new ERR_UNHANDLED_ERROR();

    if (typeof er === 'object') {
      er.domainEmitter = this;
      ObjectDefineProperty(er, 'domain', {
        __proto__: null,
        configurable: true,
        enumerable: false,
        value: domain,
        writable: true,
      });
      er.domainThrown = false;
    }

    // Remove the current domain (and its duplicates) from the domains stack and
    // set the active domain to its parent (if any) so that the domain's error
    // handler doesn't run in its own context. This prevents any event emitter
    // created or any exception thrown in that error handler from recursively
    // executing that error handler.
    const store = getDomainStore();
    const stack = store?.stack ?? [];
    const origDomainsStack = ArrayPrototypeSlice(stack);
    const origActiveDomain = store?.domain ?? null;

    // Travel the domains stack from top to bottom to find the first domain
    // instance that is not a duplicate of the current active domain.
    let idx = stack.length - 1;
    while (idx > -1 && origActiveDomain === stack[idx]) {
      --idx;
    }

    // Change the stack to not contain the current active domain, and only the
    // domains above it on the stack.
    let newStack;
    if (idx < 0) {
      newStack = [];
    } else {
      newStack = ArrayPrototypeSlice(stack, 0, idx + 1);
    }

    // Change the current active domain
    const newActiveDomain = newStack.length > 0 ? newStack[newStack.length - 1] : null;
    if (store) {
      setDomainStore({ domain: newActiveDomain, stack: newStack });
    }
    exports.active = newActiveDomain;

    updateExceptionCapture();

    domain.emit('error', er);

    // Now that the domain's error handler has completed, restore the domains
    // stack and the active domain to their original values.
    if (store) {
      setDomainStore({ domain: origActiveDomain, stack: origDomainsStack });
    }
    exports.active = origActiveDomain;
    updateExceptionCapture();

    return false;
  }

  domain.enter();
  const ret = ReflectApply(eventEmit, this, args);
  domain.exit();

  return ret;
};
