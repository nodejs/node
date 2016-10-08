'use strict'

function isArguments (object) {
  return Object.prototype.toString.call(object) === '[object Arguments]'
}

module.exports = shallower

function shallower (a, b) {
  return shallower_(a, b, [], [])
}

try {
  shallower.fastEqual = require('buffertools').equals
} catch (e) {
  // whoops, nobody told buffertools it wasn't installed
}

/**
 * This is a structural equality test, modeled on bits and pieces of loads of
 * other implementations of this algorithm, most notably the much stricter
 * `deeper`, from which this comment was copied.
 *
 * Everybody who writes one of these functions puts the documentation
 * inline, which makes it incredibly hard to follow. Here's what this version
 * of the algorithm does, in order:
 *
 * 1. Use loose equality (`==`) only for value types (non-objects). This is the
 *    biggest difference between `only-shallow` and `deeper` / `deepest` (along
 *    with being more of a duck-typer, because it doesn't care about constructor
 *    matching), and it needs to be careful to filter out objects (including
 *    Arrays). This will also catch functions, with the useful (default) property
 *    that only references to the same function are considered equal. 'Ware the
 *    halting problem!
 * 2. `null` *is* an object – a singleton value object, in fact – so if
 *    either is `null`, return a == b. For the purposes of `only-shallow`,
 *    loose testing of emptiness makes sense.
 * 3. Since the only way to make it this far is for `a` or `b` to be an object, if
 *    `a` or `b` is *not* an object, they're clearly not the same.
 * 4. It's much faster to compare dates by numeric value than by lexical value.
 * 5. Same goes for Regexps.
 * 6. The parts of an arguments list most people care about are the arguments
 *    themselves, not the callee, which you shouldn't be looking at anyway.
 * 7. Objects are more complex:
 *    a. Return `true` if `a` and `b` both have no properties.
 *    b. Ensure that `a` and `b` have the same number of own properties with the
 *       same names (which is what `Object.keys()` returns).
 *    c. Ensure that cyclical references don't blow up the stack.
 *    d. Ensure that all the key names match (faster).
 *    e. Ensure that all of the associated values match, recursively (slower).
 */
function shallower_ (a, b, ca, cb) {
  /*eslint eqeqeq:0*/
  if (typeof a !== 'object' && typeof b !== 'object' && a == b) {
    return true
  } else if (a === null || b === null) {
    return a == b
  } else if (typeof a !== 'object' || typeof b !== 'object') {
    return false
  } else if (Buffer.isBuffer(a) && Buffer.isBuffer(b)) {
    if (a.equals) {
      return a.equals(b)
    } else if (shallower.fastEqual) {
      return shallower.fastEqual.call(a, b)
    } else {
      if (a.length !== b.length) return false

      for (var j = 0; j < a.length; j++) if (a[j] != b[j]) return false

      return true
    }
  } else if (a instanceof Date && b instanceof Date) {
    return a.getTime() === b.getTime()
  } else if (a instanceof RegExp && b instanceof RegExp) {
    return a.source === b.source &&
    a.global === b.global &&
    a.multiline === b.multiline &&
    a.lastIndex === b.lastIndex &&
    a.ignoreCase === b.ignoreCase
  } else if (isArguments(a) || isArguments(b)) {
    var slice = Array.prototype.slice
    return shallower_(slice.call(a), slice.call(b), ca, cb)
  } else {
    var ka = Object.keys(a)
    var kb = Object.keys(b)
    // don't bother with stack acrobatics if there's nothing there
    if (ka.length === 0 && kb.length === 0) return true
    if (ka.length !== kb.length) return false

    var cal = ca.length
    while (cal--) if (ca[cal] === a) return cb[cal] === b
    ca.push(a); cb.push(b)

    ka.sort(); kb.sort()
    for (var k = ka.length - 1; k >= 0; k--) if (ka[k] !== kb[k]) return false

    var key
    for (var l = ka.length - 1; l >= 0; l--) {
      key = ka[l]
      if (!shallower_(a[key], b[key], ca, cb)) return false
    }

    ca.pop(); cb.pop()

    return true
  }
}
