'use strict';

module.exports = {
  wrap,
  Promise,
  setup
};

const defaultResolver = {
  resolve(xs) {
    return xs;
  }
};

var resolver = defaultResolver;

function wrap(promise) {
  return resolver.resolve(promise);
}

function setResolver(wrapper) {
  resolver = wrapper || defaultResolver;
}

function setup(process) {
  var implSet = false;
  var where = null;
  process.setPromiseImplementation = function setImpl(impl) {
    const assert = require('assert');
    const util = require('internal/util');
    assert(
      (typeof impl === 'object' || typeof impl === 'function') && impl,
      'implementation should be a truthy object'
    );
    assert(
      typeof impl.resolve === 'function',
      'implementation should have a .resolve method'
    );
    const test = impl.resolve();
    assert(
      test && typeof test.then === 'function',
      '.resolve should return a thenable'
    );
    if (!implSet) {
      implSet = true;
      where = new Error('setPromiseImplementation');
      process.setPromiseImplementation = util.deprecate(
        setImpl,
        'setPromiseImplementation has already been called for this process:\n' +
        where.stack + '\n'
      );
    }
    setResolver(impl);
  };
}
