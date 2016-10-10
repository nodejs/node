var tap = require('tap')
var test = tap.test
var match = require('../')

test("shouldn't care about key order and types", function (t) {
  t.ok(match({ a: 1, b: 2 }, { b: 2, a: '1' }))
  t.end()
})

test('should notice objects with different shapes', function (t) {
  t.notOk(match(
    { a: 1, b: 'a thing' },
    { a: 1, b: undefined }
  ))
  t.ok(match(
    { a: 1 },
    { a: 1, b: undefined }
  ))
  t.end()
})

test('extra keys in object are ok', function (t) {
  t.ok(match(
    { a: 1, b: null, c: 'ok' },
    { a: 1, b: undefined }
  ))
  t.end()
})

test('should notice objects with different keys', function (t) {
  t.notOk(match(
    { a: 1, b: 2 },
    { a: 1, c: 2 }
  ))
  t.end()
})

test('should handle dates', function (t) {
  t.notOk(match(new Date('1972-08-01'), null))
  t.notOk(match(new Date('1972-08-01'), undefined))
  t.ok(match(new Date('1972-08-01'), new Date('1972-08-01')))
  t.ok(match({ x: new Date('1972-08-01') }, { x: new Date('1972-08-01') }))
  t.end()
})

test('should handle RegExps', function (t) {
  t.notOk(match(/[b]/, /[a]/))
  t.notOk(match(/[a]/i, /[a]/g))
  t.ok(match(/[a]/, /[a]/))
  t.ok(match(/ab?[a-z]{,6}/g, /ab?[a-z]{,6}/g))
  t.end()
})

test('should handle functions', function (t) {
  var fnA = function (a) { return a }
  var fnB = function (a) { return a }

  t.notOk(match(
    function a () {},
    function a () {} // but is it the _same_ a tho
  ))
  t.notOk(match(fnA, fnB))
  t.ok(match(fnA, fnA))
  t.ok(match(fnB, fnB))
  t.end()
})

test('should handle arguments', function (t) {
  var outer = arguments
  ;(function (tt) {
    var inner = arguments
    t.ok(match(outer, outer))
    t.ok(match(outer, inner))
    t.ok(match(outer, [t]))
  }(t))
  t.end()
})

test('same arrays match', function (t) {
  t.ok(match([1, 2, 3], [1, 2, 3]))
  t.end()
})

test("different arrays don't match", function (t) {
  t.notOk(match([1, 2, 3], [1, 2, 3, 4]))
  t.notOk(match([1, 2, 3], [1, 2, 4]))
  t.end()
})

test('empty arrays match', function (t) {
  t.ok(match([], []))
  t.ok(match({ x: [] }, { x: [] }))
  t.end()
})

test("shallower shouldn't care about key order recursively and types", function (t) {
  t.ok(match(
    { x: { a: 1, b: 2 }, y: { c: 3, d: 4 } },
    { y: { d: 4, c: 3 }, x: { b: '2', a: '1' } }
  ))
  t.end()
})

test('undefined is the same as itself', function (t) {
  t.ok(match(undefined, undefined))
  t.ok(match({ x: undefined }, { x: undefined }))
  t.ok(match({ x: [undefined] }, { x: [undefined] }))
  t.end()
})

test('undefined and null are Close Enough', function (t) {
  t.ok(match(undefined, null))
  t.ok(match({ x: null }, { x: undefined }))
  t.ok(match({ x: [undefined] }, { x: [null] }))
  t.end()
})

test("null is as shallow as you'd expect", function (t) {
  t.ok(match(null, null))
  t.ok(match({ x: null }, { x: null }))
  t.ok(match({ x: [null] }, { x: [null] }))
  t.end()
})

test('the same number matches', function (t) {
  t.ok(match(0, 0))
  t.ok(match(1, 1))
  t.ok(match(3.14, 3.14))
  t.end()
})

test("different numbers don't match", function (t) {
  t.notOk(match(0, 1))
  t.notOk(match(1, -1))
  t.notOk(match(3.14, 2.72))
  t.end()
})

test("tmatch shouldn't care about key order (but still might) and types", function (t) {
  t.ok(match(
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

test("match shouldn't blow up on circular data structures", function (t) {
  var x1 = { z: 4 }
  var y1 = { x: x1 }
  x1.y = y1

  var x2 = { z: 4 }
  var y2 = { x: x2 }
  x2.y = y2

  t.ok(match(x1, x2))
  t.end()
})

test('regexps match strings', function (t) {
  var x = { one: 'String' }
  var y = { one: /.ring$/ }
  t.ok(match(x, y))
  t.ok(match(x.one, y.one))

  x = [ 'String', 'String' ]
  y = [ /.ring$/, /.ring$/ ]
  t.ok(match(x, y))

  x = [ 'Ring', /.ring$/ ]
  y = [ /.ring$/ ]
  t.notOk(match(x, y))
  t.end()
})

test('partial strings match on indexOf', function (t) {
  var x = { one: 'String' }
  var y = { one: 'rin' }

  t.ok(match(x, y))
  t.notOk(match(y, x))
  t.end()
})

test('ctors and other fun things', function (t) {
  function Foo () {
    this._isFoo = 'foo'
  }

  t.notOk(match(new Buffer('asdf'), new Buffer('asdff')))

  var d = new Date().toISOString()

  var obj = {
    buffer: new Buffer('x'),
    date: new Date(d),
    fn: function () {},
    foo: new Foo(),
    num: 1.2,
    nan: NaN,
    bool: true,
    array: [],
    str: 'asdf',
    inf: Infinity,
    neginf: -Infinity
  }

  t.ok(match(obj, {
    buffer: Buffer,
    date: Date,
    fn: Function,
    foo: Foo,
    num: Number,
    nan: NaN,
    bool: Boolean,
    array: Array,
    str: String
  }))

  t.ok(match(obj, {
    buffer: new Buffer('x'),
    date: d,
    foo: new Foo(),
    str: 'sd'
  }))

  var buf = new Buffer('x')
  buf.equals = null
  t.ok(match(obj, {
    buffer: buf
  }))

  var buf2 = new Buffer('y')
  buf2.equals = null
  t.notOk(match(buf, buf2))

  var buf3 = new Buffer('xy')
  buf3.equals = null
  t.notOk(match(buf, buf3))

  var buf4 = new Buffer('xy')
  buf4.equals = null
  t.ok(match(buf4, buf3))

  t.notOk(match(obj, {
    inf: Number
  }))

  t.notOk(match(obj, {
    neginf: Number
  }))

  t.notOk(match(obj, {
    nan: Number
  }))

  t.end()
})

test('js WAT! array/string stuff', function (t) {
  t.notOk(match([1], 1))
  t.notOk(match(1, [1]))
  t.ok(match([1], [1]))
  var o = {}
  t.ok(match(o, o))
  t.ok(match(1, '1'))
  t.end()
})
