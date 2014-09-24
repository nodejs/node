var assert     = require("assert")
var dezalgoify = require("dezalgo")

module.exports = some

/**
 * short-circuited async Array.prototype.some implementation
 *
 * Serially evaluates a list of values from a JS array or arraylike
 * against an asynchronous predicate, terminating on the first truthy
 * value. If the predicate encounters an error, pass it to the completion
 * callback. Otherwise, pass the truthy value passed by the predicate, or
 * `false` if no truthy value was passed.
 */
function some (list, test, cb) {
  assert("length" in list, "array must be arraylike")
  assert.equal(typeof test, "function", "predicate must be callable")
  assert.equal(typeof cb, "function", "callback must be callable")

  var array   = slice(list)
    , index   = 0
    , length  = array.length
    , hecomes = dezalgoify(cb)

  map()

  function map () {
    if (index >= length) return hecomes(null, false)

    test(array[index], reduce)
  }

  function reduce (er, result) {
    if (er) return hecomes(er, false)
    if (result) return hecomes(null, result)

    index++
    map()
  }
}

// Array.prototype.slice on arguments arraylike is expensive
function slice(args) {
  var l = args.length, a = [], i
  for (i = 0; i < l; i++) a[i] = args[i]
  return a
}
