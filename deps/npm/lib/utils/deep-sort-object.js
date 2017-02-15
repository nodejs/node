'use strict'
var sortedObject = require('sorted-object')

module.exports = function deepSortObject (obj, sortBy) {
  if (obj == null || typeof obj !== 'object') return obj
  if (obj instanceof Array) {
    return obj.map(function (x) {
      return deepSortObject(x, sortBy)
    })
  }
  obj = sortedObject(obj)
  Object.keys(obj).forEach(function (key) {
    obj[key] = deepSortObject(obj[key], sortBy)
  })
  return obj
}
