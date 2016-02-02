'use strict';

// This module patches the global Promise before any other code
// has access to it. It does this so that promises can be instrumented
// with domains and async_wrap as expected. It is a temporary API -
// once V8 offers a way for us to interact with the MicrotaskQueue
// it can fall off.
module.exports = {
  addAsyncHooks,
  addDomainHooks
};

const BuiltinPromise = Promise;

Promise = function Promise (resolver)  {
  const base = new BuiltinPromise(resolver);
  domainInitHook(base, process);
  asyncInitHook(base);
  return base;
}

// Copy class methods.
const props = [
  'accept',
  'reject',
  'all',
  'resolve',
  'defer',
  'race'
];

props.forEach((xs) => {
  Promise[xs] = BuiltinPromise[xs];
});

// This is so folks doing `Promise.prototype.then.call` get the right
// function.
Promise.prototype = Object.assign(Promise.prototype, BuiltinPromise.prototype);

const originalThen = BuiltinPromise.prototype.then;
BuiltinPromise.prototype.then = function (success, failure) {
  // XXX(chrisdickinson): We use a context object because we've got to
  // wrap the function _before_ we get the child promise; but the wrapped
  // function has to know about the child promise so async_wrap works as
  // expected.
  const context = {promise: null};
  switch (arguments.length) {
    case 2:
      success = domainWrapHook(this, success);
      success = asyncWrapHook(this, success, context);
      failure = domainWrapHook(this, failure);
      failure = asyncWrapHook(this, failure, context);
      context.promise = originalThen.call(this, success, failure);
      domainInitHook(context.promise, this);
      asyncInitHook(context.promise);
      break;
    case 1:
      success = domainWrapHook(this, success);
      success = asyncWrapHook(this, success, context);
      context.promise = originalThen.call(this, success);
      domainInitHook(context.promise, this);
      asyncInitHook(context.promise);
      break;
    case 0:
      context.promise = originalThen.call(this);
      break;
  }
  domainInitHook(context.promise, this);
  asyncInitHook(context.promise);
  return context.promise;
};

var domainInitHook = _noopInit;
var domainWrapHook = _noopWrap;
var asyncInitHook = _noopInit;
var asyncWrapHook = _noopWrap;

function addAsyncHooks (hooks) {
  asyncInitHook = hooks.init || asyncInitHook;
  asyncWrapHook = hooks.wrap || asyncWrapHook;
}

function addDomainHooks (hooks) {
  domainInitHook = hooks.init || domainInitHook;
  domainWrapHook = hooks.wrap || domainWrapHook;
}

function _noopInit (promise) {
  return;
}

function _noopWrap (promise, func, context) {
  return func;
}
