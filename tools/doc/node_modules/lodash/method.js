var baseInvoke = require('./internal/baseInvoke'),
    rest = require('./rest');

/**
 * Creates a function that invokes the method at `path` of a given object.
 * Any additional arguments are provided to the invoked method.
 *
 * @static
 * @memberOf _
 * @category Util
 * @param {Array|string} path The path of the method to invoke.
 * @param {...*} [args] The arguments to invoke the method with.
 * @returns {Function} Returns the new function.
 * @example
 *
 * var objects = [
 *   { 'a': { 'b': { 'c': _.constant(2) } } },
 *   { 'a': { 'b': { 'c': _.constant(1) } } }
 * ];
 *
 * _.map(objects, _.method('a.b.c'));
 * // => [2, 1]
 *
 * _.invokeMap(_.sortBy(objects, _.method(['a', 'b', 'c'])), 'a.b.c');
 * // => [1, 2]
 */
var method = rest(function(path, args) {
  return function(object) {
    return baseInvoke(object, path, args);
  };
});

module.exports = method;
