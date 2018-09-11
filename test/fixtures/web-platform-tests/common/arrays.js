// Returns true if the given arrays are equal. Optionally can pass an equality function.
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
