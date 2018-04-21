var LazyWrapper = require('./LazyWrapper'),
    arrayPush = require('./arrayPush');

/**
 * The base implementation of `wrapperValue` which returns the result of
 * performing a sequence of actions on the unwrapped `value`, where each
 * successive action is supplied the return value of the previous.
 *
 * @private
 * @param {*} value The unwrapped value.
 * @param {Array} actions Actions to peform to resolve the unwrapped value.
 * @returns {*} Returns the resolved value.
 */
function baseWrapperValue(value, actions) {
  var result = value;
  if (result instanceof LazyWrapper) {
    result = result.value();
  }
  var index = -1,
      length = actions.length;

  while (++index < length) {
    var action = actions[index];
    result = action.func.apply(action.thisArg, arrayPush([result], action.args));
  }
  return result;
}

module.exports = baseWrapperValue;
