'use strict';

const {
  ObjectDefineProperties,
  ObjectDefineProperty,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_MISSING_OPTION,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

const {
  customInspect,
} = require('internal/webstreams/util');

const {
  validateObject,
} = require('internal/validators');

/**
 * @callback QueuingStrategySize
 * @param {any} chunk
 * @returns {number}
 */

/**
 * @typedef {{
 *   highWaterMark : number,
 *   size? : QueuingStrategySize,
 * }} QueuingStrategy
 */

const nameDescriptor = { __proto__: null, value: 'size' };
const byteSizeFunction = ObjectDefineProperty(
  (chunk) => chunk.byteLength,
  'name',
  nameDescriptor,
);
const countSizeFunction = ObjectDefineProperty(() => 1, 'name', nameDescriptor);

const getNonWritablePropertyDescriptor = (value) => {
  return {
    __proto__: null,
    configurable: true,
    value,
  };
};

/**
 * @type {QueuingStrategy}
 */
class ByteLengthQueuingStrategy {
  #state;
  #byteSizeFunction = byteSizeFunction;

  /**
   * @param {{
   *   highWaterMark : number
   * }} init
   */
  constructor(init) {
    validateObject(init, 'init');
    if (init.highWaterMark === undefined)
      throw new ERR_MISSING_OPTION('init.highWaterMark');

    // The highWaterMark value is not checked until the strategy
    // is actually used, per the spec.
    this.#state = {
      highWaterMark: +init.highWaterMark,
    };
  }

  /**
   * @readonly
   * @type {number}
   */
  get highWaterMark() {
    return this.#state.highWaterMark;
  }

  /**
   * @type {QueuingStrategySize}
   */
  get size() {
    return this.#byteSizeFunction;
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, 'ByteLengthQueuingStrategy', {
      highWaterMark: this.highWaterMark,
    });
  }
}

ObjectDefineProperties(ByteLengthQueuingStrategy.prototype, {
  highWaterMark: kEnumerableProperty,
  size: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(ByteLengthQueuingStrategy.name),
});

/**
 * @type {QueuingStrategy}
 */
class CountQueuingStrategy {
  #state;
  #countSizeFunction = countSizeFunction;

  /**
   * @param {{
   *   highWaterMark : number
   * }} init
   */
  constructor(init) {
    validateObject(init, 'init');
    if (init.highWaterMark === undefined)
      throw new ERR_MISSING_OPTION('init.highWaterMark');

    // The highWaterMark value is not checked until the strategy
    // is actually used, per the spec.
    this.#state = {
      highWaterMark: +init.highWaterMark,
    };
  }

  /**
   * @readonly
   * @type {number}
   */
  get highWaterMark() {
    return this.#state.highWaterMark;
  }

  /**
   * @type {QueuingStrategySize}
   */
  get size() {
    return this.#countSizeFunction;
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, 'CountQueuingStrategy', {
      highWaterMark: this.highWaterMark,
    });
  }
}

ObjectDefineProperties(CountQueuingStrategy.prototype, {
  highWaterMark: kEnumerableProperty,
  size: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(CountQueuingStrategy.name),
});

module.exports = {
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
};
