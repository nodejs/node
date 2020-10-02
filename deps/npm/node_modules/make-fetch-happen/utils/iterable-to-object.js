'use strict'

module.exports = function iterableToObject (iter) {
  const obj = {}
  for (const k of iter.keys()) {
    obj[k] = iter.get(k)
  }
  return obj
}
