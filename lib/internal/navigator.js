'use strict';

const {
  ObjectDefineProperties,
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
  #userAgent = `Node.js/${nodeVersion.slice(1, nodeVersion.indexOf('.'))}`;

  constructor() {
    if (arguments[0] === kInitialize) {
      return;
    }
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  /**
   * @return {'Mozilla'}
   */
  get appCodeName() {
    return 'Mozilla';
  }

  /**
   * @return {'Netscape'}
   */
  get appName() {
    return 'Netscape';
  }

  /**
   * @return {string}
   */
  get appVersion() {
    return this.#userAgent;
  }

  /**
   * @return {number}
   */
  get hardwareConcurrency() {
    this.#availableParallelism ??= getAvailableParallelism();
    return this.#availableParallelism;
  }

  /**
   * @return {'Gecko'}
   */
  get product() {
    return 'Gecko';
  }

  /**
   * @return {'20030107'}
   */
  get productSub() {
    return '20030107';
  }

  /**
   * @return {string}
   */
  get userAgent() {
    return this.#userAgent;
  }

  /**
   * @return {''}
   */
  get vendor() {
    return '';
  }

  /**
   * @return {''}
   */
  get vendorSub() {
    return '';
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
