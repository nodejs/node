'use strict';

module.exports = {
  wrap,
  setResolver,
  Promise
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
