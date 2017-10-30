var arrayEvery = require('../internal/arrayEvery'),
    baseFlatten = require('../internal/baseFlatten'),
    baseIsFunction = require('../internal/baseIsFunction'),
    restParam = require('./restParam');

/** Used as the `TypeError` message for "Functions" methods. */
var FUNC_ERROR_TEXT = 'Expected a function';

/* Native method references for those with the same name as other `lodash` methods. */
var nativeMin = Math.min;

/**
 * Creates a function that runs each argument through a corresponding
 * transform function.
 *
 * @static
 * @memberOf _
 * @category Function
 * @param {Function} func The function to wrap.
 * @param {...(Function|Function[])} [transforms] The functions to transform
 * arguments, specified as individual functions or arrays of functions.
 * @returns {Function} Returns the new function.
 * @example
 *
 * function doubled(n) {
 *   return n * 2;
 * }
 *
 * function square(n) {
 *   return n * n;
 * }
 *
 * var modded = _.modArgs(function(x, y) {
 *   return [x, y];
 * }, square, doubled);
 *
 * modded(1, 2);
 * // => [1, 4]
 *
 * modded(5, 10);
 * // => [25, 20]
 */
var modArgs = restParam(function(func, transforms) {
  transforms = baseFlatten(transforms);
  if (typeof func != 'function' || !arrayEvery(transforms, baseIsFunction)) {
    throw new TypeError(FUNC_ERROR_TEXT);
  }
  var length = transforms.length;
  return restParam(function(args) {
    var index = nativeMin(args.length, length);
    while (index--) {
      args[index] = transforms[index](args[index]);
    }
    return func.apply(this, args);
  });
});

module.exports = modArgs;
