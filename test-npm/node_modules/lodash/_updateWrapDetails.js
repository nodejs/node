var arrayEach = require('./_arrayEach'),
    arrayIncludes = require('./_arrayIncludes');

/** Used to compose bitmasks for function metadata. */
var BIND_FLAG = 1,
    BIND_KEY_FLAG = 2,
    CURRY_FLAG = 8,
    CURRY_RIGHT_FLAG = 16,
    PARTIAL_FLAG = 32,
    PARTIAL_RIGHT_FLAG = 64,
    ARY_FLAG = 128,
    REARG_FLAG = 256,
    FLIP_FLAG = 512;

/** Used to associate wrap methods with their bit flags. */
var wrapFlags = [
  ['ary', ARY_FLAG],
  ['bind', BIND_FLAG],
  ['bindKey', BIND_KEY_FLAG],
  ['curry', CURRY_FLAG],
  ['curryRight', CURRY_RIGHT_FLAG],
  ['flip', FLIP_FLAG],
  ['partial', PARTIAL_FLAG],
  ['partialRight', PARTIAL_RIGHT_FLAG],
  ['rearg', REARG_FLAG]
];

/**
 * Updates wrapper `details` based on `bitmask` flags.
 *
 * @private
 * @returns {Array} details The details to modify.
 * @param {number} bitmask The bitmask flags. See `createWrap` for more details.
 * @returns {Array} Returns `details`.
 */
function updateWrapDetails(details, bitmask) {
  arrayEach(wrapFlags, function(pair) {
    var value = '_.' + pair[0];
    if ((bitmask & pair[1]) && !arrayIncludes(details, value)) {
      details.push(value);
    }
  });
  return details.sort();
}

module.exports = updateWrapDetails;
