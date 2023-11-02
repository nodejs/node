'use strict';

const {
  ObjectDefineProperties,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  Symbol,
} = primordials;

const {
  ERR_ILLEGAL_CONSTRUCTOR,
} = require('internal/errors').codes;

const {
  kEnumerableProperty,
} = require('internal/util');

const {
  getAvailableParallelism,
} = internalBinding('os');

const kInitialize = Symbol('kInitialize');
const nodeVersion = process.version;

class Navigator {
  // Private properties are used to avoid brand validations.
  #availableParallelism;
  #userAgent = `Node.js/${StringPrototypeSlice(nodeVersion, 1, StringPrototypeIndexOf(nodeVersion, '.'))}`;

  constructor() {
    if (arguments[0] === kInitialize) {
      return;
    }
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  /**
   * @return {number}
   */
  get hardwareConcurrency() {
    this.#availableParallelism ??= getAvailableParallelism();
    return this.#availableParallelism;
  }

  /**
   * @return {string}
   */
  get userAgent() {
    return this.#userAgent;
  }
}

ObjectDefineProperties(Navigator.prototype, {
  hardwareConcurrency: kEnumerableProperty,
  userAgent: kEnumerableProperty,
});

module.exports = {
  navigator: new Navigator(kInitialize),
  Navigator,
};
