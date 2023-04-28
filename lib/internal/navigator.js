'use strict';

const {
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
} = primordials;

const {
  ERR_ILLEGAL_CONSTRUCTOR,
} = require('internal/errors').codes;

const {
  getAvailableParallelism,
} = internalBinding('os');

class Navigator {
  constructor() {
    throw ERR_ILLEGAL_CONSTRUCTOR();
  }
}

class InternalNavigator {}
InternalNavigator.prototype.constructor = Navigator.prototype.constructor;
ObjectSetPrototypeOf(InternalNavigator.prototype, Navigator.prototype);

ObjectDefineProperties(Navigator.prototype, {
  hardwareConcurrency: {
    __proto__: null,
    configurable: true,
    enumerable: true,
    get() {
      return getAvailableParallelism();
    },
  },
});

module.exports = {
  navigator: new InternalNavigator(),
  Navigator,
};
