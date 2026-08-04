/**
 * Callback for checking equality of c and d.
 *
 * @callback equalityCallback
 * @param {*} c
 * @param {*} d
 * @returns {boolean}
 */

/**
 * Returns true if the given arrays are equal. Optionally can pass an equality function.
 * @param {Array} a
 * @param {Array} b
 * @param {equalityCallback} callbackFunction - defaults to `c === d`
 * @returns {boolean}
 */
export function areArraysEqual(a, b, equalityFunction = (c, d) => { return c === d; }) {
  try {
    if (a.length !== b.length)
      return false;

    for (let i = 0; i < a.length; i++) {
      if (!equalityFunction(a[i], b[i]))
        return false;
    }
  } catch (ex) {
    return false;
  }

  return true;
}
