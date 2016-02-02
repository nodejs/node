'use strict';

// This module patches the global Promise before any other code has access to
// it. It does this so that promises can be instrumented with domains as
// expected. It is a temporary API - once V8 offers a way for us to interact
// with the MicrotaskQueue it can fall off.
module.exports = {
  addDomainHook
};

const originalThen = Promise.prototype.then;
Promise.prototype.then = function (success, failure) {
  var promise;
  switch (arguments.length) {
    case 2:
      if (typeof success === 'function') {
        success = domainWrapHook(success);
      }
      if (typeof failure === 'function') {
        failure = domainWrapHook(failure);
      }
      promise = originalThen.call(this, success, failure);
      break;
    case 1:
      if (typeof success === 'function') {
        success = domainWrapHook(success);
      }
      promise = originalThen.call(this, success);
      break;
    case 0:
      promise = originalThen.call(this);
      break;
  }
  return promise;
};

var domainWrapHook = _noopDomainWrap;

function addDomainHook (hook) {
  domainWrapHook = hook;
}

function _noopDomainWrap (func) {
  return func;
}
