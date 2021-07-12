'use strict';

const {
  ObjectDefineProperties,
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
} = require('internal/util');

const {
  customInspect,
  isBrandCheck,
  kType,
  kState,
  kEnumerableProperty,
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
 *
 * @typedef {{
 *   highWaterMark : number,
 *   size? : QueuingStrategySize,
 * }} QueuingStrategy
 */

// eslint-disable-next-line func-name-matching,func-style
const byteSizeFunction = function size(chunk) { return chunk.byteLength; };

// eslint-disable-next-line func-name-matching,func-style
const countSizeFunction = function size() { return 1; };

/**
 * @type {QueuingStrategy}
 */
class ByteLengthQueuingStrategy {
  [kType] = 'ByteLengthQueuingStrategy';

  get [SymbolToStringTag]() { return this[kType]; }

  /**
   * @param {{
   *   highWaterMark : number
   * }} init
   */
  constructor(init) {
    validateObject(init, 'init');
    if (init.highWaterMark === undefined)
      throw new ERR_MISSING_OPTION('options.highWaterMark');

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
});

/**
 * @type {QueuingStrategy}
 */
class CountQueuingStrategy {
  [kType] = 'CountQueuingStrategy';

  get [SymbolToStringTag]() { return this[kType]; }

  /**
   * @param {{
   *   highWaterMark : number
   * }} init
   */
  constructor(init) {
    validateObject(init, 'init');
    if (init.highWaterMark === undefined)
      throw new ERR_MISSING_OPTION('options.highWaterMark');

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
});

module.exports = {
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
};
