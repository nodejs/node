'use strict';

const {
  ObjectDefineProperties,
  ObjectDefineProperty,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_THIS,
    ERR_MISSING_OPTION,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

const {
  customInspect,
  isBrandCheck,
  kType,
  kState,
} = require('internal/webstreams/util');

const {
  validateObject,
} = require('internal/validators');

const isByteLengthQueuingStrategy =
  isBrandCheck('ByteLengthQueuingStrategy');

const isCountQueuingStrategy =
  isBrandCheck('CountQueuingStrategy');

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
  [kType] = 'ByteLengthQueuingStrategy';

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
    this[kState] = {
      highWaterMark: +init.highWaterMark,
    };
  }

  /**
   * @readonly
   * @type {number}
   */
  get highWaterMark() {
    if (!isByteLengthQueuingStrategy(this))
      throw new ERR_INVALID_THIS('ByteLengthQueuingStrategy');
    return this[kState].highWaterMark;
  }

  /**
   * @type {QueuingStrategySize}
   */
  get size() {
    if (!isByteLengthQueuingStrategy(this))
      throw new ERR_INVALID_THIS('ByteLengthQueuingStrategy');
    return byteSizeFunction;
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
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
  [kType] = 'CountQueuingStrategy';

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
    this[kState] = {
      highWaterMark: +init.highWaterMark,
    };
  }

  /**
   * @readonly
   * @type {number}
   */
  get highWaterMark() {
    if (!isCountQueuingStrategy(this))
      throw new ERR_INVALID_THIS('CountQueuingStrategy');
    return this[kState].highWaterMark;
  }

  /**
   * @type {QueuingStrategySize}
   */
  get size() {
    if (!isCountQueuingStrategy(this))
      throw new ERR_INVALID_THIS('CountQueuingStrategy');
    return countSizeFunction;
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
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
