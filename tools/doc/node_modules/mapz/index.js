'use strict'

var array = require('x-is-array')

module.exports = factory

/* Functional map with sugar. */
function factory(fn, options) {
  var settings = options || {}
  var key = settings.key
  var indices = settings.indices
  var gapless = settings.gapless

  if (typeof settings === 'string') {
    key = settings
  }

  if (indices == null) {
    indices = true
  }

  return all

  function all(values) {
    var results = []
    var parent = values
    var index = -1
    var length
    var result

    if (key) {
      if (array(values)) {
        parent = null
      } else {
        values = parent[key]
      }
    }

    length = values.length

    while (++index < length) {
      if (indices) {
        result = fn.call(this, values[index], index, parent)
      } else {
        result = fn.call(this, values[index], parent)
      }

      if (!gapless || result != null) {
        results.push(result)
      }
    }

    return results
  }
}
