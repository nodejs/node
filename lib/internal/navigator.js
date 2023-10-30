'use strict';

const {
  NumberParseFloat,
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
  getTotalMem,
} = internalBinding('os');

const kInitialize = Symbol('kInitialize');
const nodeVersion = process.version;

function getApproximatedDeviceMemory(totalMemoryInBytes) {
  const totalMemoryInMB = totalMemoryInBytes / (1024 * 1024);
  let mostSignificantByte = 0;
  let lowerBound = totalMemoryInMB;

  // Extract the most-significant-bit and its location.
  while (lowerBound > 1) {
    lowerBound >>= 1;
    mostSignificantByte++;
  }

  const upperBound = (lowerBound + 1) << mostSignificantByte;
  lowerBound = lowerBound << mostSignificantByte;

  // Find the closest bound, and convert it to GB.
  if (totalMemoryInMB - lowerBound <= upperBound - totalMemoryInMB) {
    return NumberParseFloat((lowerBound / 1024.0).toFixed(3));
  }
  return NumberParseFloat((upperBound / 1024.0).toFixed(3));
}

class Navigator {
  // Private properties are used to avoid brand validations.
  #availableParallelism;
  #userAgent = `Node.js/${nodeVersion.slice(1, nodeVersion.indexOf('.'))}`;
  #deviceMemory = getApproximatedDeviceMemory(getTotalMem());

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

  /**
   * @returns {number}
   */
  get deviceMemory() {
    return this.#deviceMemory;
  }
}

ObjectDefineProperties(Navigator.prototype, {
  hardwareConcurrency: kEnumerableProperty,
  userAgent: kEnumerableProperty,
  deviceMemory: kEnumerableProperty,
});

module.exports = {
  getApproximatedDeviceMemory,
  navigator: new Navigator(kInitialize),
  Navigator,
};
