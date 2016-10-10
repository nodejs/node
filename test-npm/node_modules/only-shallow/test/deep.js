var tap = require('tap')
var test = tap.test
var same = require('../')

test("shouldn't care about key order and types", function (t) {
  t.ok(same({ a: 1, b: 2 }, { b: 2, a: '1' }))
  t.end()
})

test('should notice objects with different shapes', function (t) {
  t.notOk(same(
    { a: 1 },
    { a: 1, b: undefined }
  ))
  t.end()
})

test('should notice objects with different keys', function (t) {
  t.notOk(same(
    { a: 1, b: 2 },
    { a: 1, c: 2 }
  ))
  t.end()
})

test('should handle dates', function (t) {
  t.notOk(same(new Date('1972-08-01'), null))
  t.notOk(same(new Date('1972-08-01'), undefined))
  t.ok(same(new Date('1972-08-01'), new Date('1972-08-01')))
  t.ok(same({ x: new Date('1972-08-01') }, { x: new Date('1972-08-01') }))
  t.end()
})

test('should handle RegExps', function (t) {
  t.notOk(same(/[b]/, /[a]/))
  t.notOk(same(/[a]/i, /[a]/g))
  t.ok(same(/[a]/, /[a]/))
  t.ok(same(/ab?[a-z]{,6}/g, /ab?[a-z]{,6}/g))
  t.end()
})

test('should handle functions', function (t) {
  var fnA = function (a) { return a }
  var fnB = function (a) { return a }

  t.notOk(same(
    function a () {},
    function a () {} // but is it the _same_ a tho
  ))
  t.notOk(same(fnA, fnB))
  t.ok(same(fnA, fnA))
  t.ok(same(fnB, fnB))
  t.end()
})

test('should handle arguments', function (t) {
  var outer = arguments
  ;(function inner (tt) {
    var inner = arguments
    t.ok(same(outer, outer))
    t.ok(same(outer, inner))
    t.ok(same(outer, [t]))
  }(t))
  t.end()
})

test('same arrays match', function (t) {
  t.ok(same([1, 2, 3], [1, 2, 3]))
  t.end()
})

test("different arrays don't match", function (t) {
  t.notOk(same([1, 2, 3], [1, 2, 3, 4]))
  t.notOk(same([1, 2, 3], [1, 2, 4]))
  t.end()
})

test('empty arrays match', function (t) {
  t.ok(same([], []))
  t.ok(same({ x: [] }, { x: [] }))
  t.end()
})

test("shallower shouldn't care about key order recursively and types", function (t) {
  t.ok(same(
    { x: { a: 1, b: 2 }, y: { c: 3, d: 4 } },
    { y: { d: 4, c: 3 }, x: { b: '2', a: '1' } }
  ))
  t.end()
})

test('undefined is the same as itself', function (t) {
  t.ok(same(undefined, undefined))
  t.ok(same({ x: undefined }, { x: undefined }))
  t.ok(same({ x: [undefined] }, { x: [undefined] }))
  t.end()
})

test('undefined and null are Close Enough', function (t) {
  t.ok(same(undefined, null))
  t.ok(same({ x: null }, { x: undefined }))
  t.ok(same({ x: [undefined] }, { x: [null] }))
  t.end()
})

test("null is as shallow as you'd expect", function (t) {
  t.ok(same(null, null))
  t.ok(same({ x: null }, { x: null }))
  t.ok(same({ x: [null] }, { x: [null] }))
  t.end()
})

test('the same number matches', function (t) {
  t.ok(same(0, 0))
  t.ok(same(1, 1))
  t.ok(same(3.14, 3.14))
  t.end()
})

test("different numbers don't match", function (t) {
  t.notOk(same(0, 1))
  t.notOk(same(1, -1))
  t.notOk(same(3.14, 2.72))
  t.end()
})

test("shallower shouldn't care about key order (but still might) and types", function (t) {
  t.ok(same(
    [
      { foo: { z: 100, y: 200, x: 300 } },
      'bar',
      11,
      { baz: { d: 4, a: 1, b: 2, c: 3 } }
    ],
    [
      { foo: { z: 100, y: 200, x: 300 } },
      'bar',
      11,
      { baz: { a: '1', b: '2', c: '3', d: '4' } }
    ]
  ))
  t.end()
})

test("shallower shouldn't blow up on circular data structures", function (t) {
  var x1 = { z: 4 }
  var y1 = { x: x1 }
  x1.y = y1

  var x2 = { z: 4 }
  var y2 = { x: x2 }
  x2.y = y2

  t.ok(same(x1, x2))
  t.end()
})
