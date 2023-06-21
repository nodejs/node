'use strict';

const {
  ObjectDefineProperties,
  ReflectConstruct,
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

class Navigator {
  // Private properties are used to avoid brand validations.
  #availableParallelism;

  constructor() {
    throw ERR_ILLEGAL_CONSTRUCTOR();
  }

  /**
   * @return {number}
   */
  get hardwareConcurrency() {
    this.#availableParallelism ??= getAvailableParallelism();
    return this.#availableParallelism;
  }
}

ObjectDefineProperties(Navigator.prototype, {
  hardwareConcurrency: kEnumerableProperty,
});

module.exports = {
  navigator: ReflectConstruct(function() {}, [], Navigator),
  Navigator,
};
