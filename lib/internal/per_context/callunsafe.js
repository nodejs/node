'use strict';

const {
  ReflectApply,
} = primordials;

class CallUnsafe {
  #method;
  #object;
  constructor(object, methodName) {
    this.#object = object;
    this.#method = object[methodName];
  }
  get is_callable() {
    return typeof this.#method === 'function';
  }
  call(...args) {
    return ReflectApply(this.#method, this.#object, args);
  }
}

function callUnsafe(object, methodName, ...args) {
  const u = new CallUnsafe(object, methodName);
  if (u.is_callable) {
    return u.call(...args);
  }
}

module.exports = {
  CallUnsafe,
  callUnsafe,
};
