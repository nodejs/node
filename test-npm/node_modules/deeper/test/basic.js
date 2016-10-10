'use strict'

var EventEmitter = require('events').EventEmitter
var test = require('tap').test
var d = require('../')

function functionA (a) { return a }
var heinous = {
  nothin: null,
  nope: undefined,
  number: 0,
  funky: functionA,
  stringer: 'heya',
  then: new Date('1981-03-30'),
  rexpy: /^(pi|π)$/,
  granular: {
    stuff: [0, 1, 2]
  }
}
heinous.granular.self = heinous

var awful = {
  nothin: null,
  nope: undefined,
  number: 0,
  funky: functionA,
  stringer: 'heya',
  then: new Date('1981-03-30'),
  rexpy: /^(pi|π)$/,
  granular: {
    stuff: [0, 1, 2]
  }
}
awful.granular.self = awful

test('deeper handles all the edge cases', function (t) {
  /*
   *
   * SUCCESS
   *
   */

  var functionB = functionA

  // 1. === gets the job done
  t.ok(d(null, null), 'null is the same as itself')
  t.ok(d(undefined, undefined), 'undefined is the same as itself')
  t.ok(d(0, 0), 'numbers check out')
  t.ok(d(1 / 0, 1 / 0), "it's a travesty that 1 / 0 = Infinity, but Infinities are equal")
  t.ok(d('ok', 'ok'), 'strings check out')
  t.ok(d(functionA, functionB), 'references to the same function are equal')

  // 4. buffers are compared by value
  var bufferA = new Buffer('abc')
  var bufferB = new Buffer('abc')
  t.ok(d(bufferA, bufferB), 'buffers are compared by value')

  // 5. dates are compared by numeric (time) value
  var dateA = new Date('2001-01-11')
  var dateB = new Date('2001-01-11')
  t.ok(d(dateA, dateB), 'dates are compared by time value')

  // 6. regexps are compared by their properties
  var rexpA = /^h[oe][wl][dl][oy]$/
  var rexpB = /^h[oe][wl][dl][oy]$/
  t.ok(d(rexpA, rexpB), 'regexps are compared by their properties')

  // 8. loads of tests for objects
  t.ok(d({}, {}), 'bare objects check out')
  var a = { a: 'a' }
  var b = a
  t.ok(d(a, b), 'identical object references check out')
  b = { a: 'a' }
  t.ok(d(a, b), 'identical simple object values check out')

  t.ok(d([0, 1], [0, 1]), 'arrays check out')

  function onerror (error) { console.err(error.stack) }
  var eeA = new EventEmitter()
  eeA.on('error', onerror)
  var eeB = new EventEmitter()
  eeB.on('error', onerror)
  t.ok(d(eeA, eeB), 'more complex objects check out')

  var cyclicA = {}
  cyclicA.x = cyclicA
  var cyclicB = {}
  cyclicB.x = cyclicB
  t.ok(d(cyclicA, cyclicB), 'can handle cyclic data structures')

  var y = {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {}}}}}}}}}}}}}}}}
  y.v.v.v.v.v.v.v.v.v.v.v.v.v.v.v.v = y
  var z = {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {v: {}}}}}}}}}}}}}}}}
  z.v.v.v.v.v.v.v.v.v.v.v.v.v.v.v.v = z
  t.ok(d(y, z), 'deeply recursive data structures also work')

  t.ok(d(heinous, awful), 'more complex objects also check out')

  awful.granular.self = heinous
  heinous.granular.self = awful
  t.ok(d(heinous, awful),
    'mutual recursion with otherwise identical structures fools deepEquals')

  /*
   *
   * FAILURE
   *
   */

  // 1. === does its job
  t.notOk(d(NaN, NaN), 'NaN is the only JavaScript value not equal to itself')
  t.notOk(d(1 / 0, -1 / 0), 'opposite infinities are different')
  t.notOk(d(1, '1'), 'strict equality, no coercion between strings and numbers')
  t.notOk(d('ok', 'nok'), 'different strings are different')
  t.notOk(d(0, '0'), 'strict equality, no coercion between strings and numbers')
  t.notOk(d(undefined, null), 'so many kinds of nothingness!')
  t.notOk(d(function nop () {},
    function nop () {}), 'functions are only the same by reference')

  // 2. one is an object, the other is not
  t.notOk(d(undefined, {}), "if both aren't objects, not the same")

  // 3. null is an object
  t.notOk(d({}, null), 'null is of type object')

  // 4. buffers are compared by both byte length (for speed) and value
  bufferB = new Buffer('abcd')
  t.notOk(d(bufferA, bufferB), 'Buffers are checked for length')
  bufferB = new Buffer('abd')
  t.notOk(d(bufferA, bufferB), 'Buffers are also checked for value')

  // 5. dates
  dateB = new Date('2001-01-12')
  t.notOk(d(dateA, dateB), 'different dates are not the same')

  // 6. regexps
  rexpB = /^(howdy|hello)$/
  t.notOk(d(rexpA, rexpB), 'different regexps are not the same')

  // 7. arguments
  var outer = arguments
  ;(function inner (tt) {
    var inner = arguments
    t.ok(d(outer, outer))
    t.ok(d(outer, inner))
    t.notOk(d(outer, [t]))
  }(t))

  // 8. objects present edge cases galore
  t.notOk(d([], {}), "different object types shouldn't match")

  var nullstructor = Object.create(null)
  t.notOk(d({}, nullstructor), 'Object.create(null).constructor === undefined')

  b = { b: 'b' }
  t.notOk(d(a, b), "different object values aren't the same")

  var c = { b: 'b', c: undefined }
  t.notOk(d(b, c), "different object values aren't the same")

  function ondata (data) { console.log(data) }
  eeB.on('data', ondata)
  t.notOk(d(eeA, eeB), "changed objects don't match")

  awful.granular.stuff[2] = 3
  t.notOk(d(heinous, awful), 'small changes should be found')

  awful.granular.stuff[2] = 2
  t.ok(d(heinous, awful), 'small changes should be fixable')

  t.end()
})
