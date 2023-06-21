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
  constructor() {
    throw ERR_ILLEGAL_CONSTRUCTOR();
  }

  /**
   * @return {number}
   */
  get hardwareConcurrency() {
    return getAvailableParallelism();
  }
}

ObjectDefineProperties(Navigator.prototype, {
  hardwareConcurrency: kEnumerableProperty,
});

module.exports = {
  navigator: ReflectConstruct(function() {}, [], Navigator),
  Navigator,
};
