var arraySome = require('./internal/arraySome'),
    createOver = require('./internal/createOver');

/**
 * Creates a function that checks if **any** of the `predicates` return
 * truthy when invoked with the arguments provided to the created function.
 *
 * @static
 * @memberOf _
 * @category Util
 * @param {...(Function|Function[])} predicates The predicates to check.
 * @returns {Function} Returns the new function.
 * @example
 *
 * var func = _.overSome(Boolean, isFinite);
 *
 * func('1');
 * // => true
 *
 * func(null);
 * // => true
 *
 * func(NaN);
 * // => false
 */
var overSome = createOver(arraySome);

module.exports = overSome;
