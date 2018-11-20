var assert = require('assert')
var parse = require('../').parse
var stringify = require('../').stringify

function deepEqual(x, y) {
  if (Number.isNaN(x)) {
    return assert(Number.isNaN(y))
  }
  assert.deepEqual(x, y)
}

function addTest(arg, arg2, arg3) {
  function fn() {
    deepEqual(parse(stringify(arg)), arg2 === undefined ? arg : arg2)
    if (arg !== undefined) deepEqual(JSON.parse(stringify(arg, {mode: 'json', indent: false})), (arg3 === undefined ? (arg2 === undefined ? arg : arg2) : arg3))
  }

  if (typeof(describe) === 'function') {
    it('test_stringify: ' + JSON.stringify(arg), fn)
  } else {
    fn()
  }
}

addTest(0)
addTest(-0)
addTest(NaN, undefined, null)
addTest(Infinity, undefined, null)
addTest(-Infinity, undefined, null)
addTest(123)
addTest(19508130958019385.135135)
addTest(-2e123)
addTest(null)
addTest(undefined)
addTest([])
addTest([,,,,,,,], [null,null,null,null,null,null,null])
addTest([undefined,null,1,2,3,], [null,null,1,2,3])
addTest([[[[]]],[[]]])
addTest({})
addTest({1:2,3:4})
addTest({1:{1:{1:{1:4}}}, 3:4})
addTest({1:undefined, 3:undefined}, {})
addTest(new Number(4), 4)
addTest(new Boolean(true), true)
addTest(new String('xqefxef'), 'xqefxef')
addTest(new Boolean(), false)

var r='';for (var i=0; i<5000; i++) {r+=String.fromCharCode(i)}
addTest(r)

assert.equal("[1, 2, 3]", stringify([1, 2, 3], {indent: 1}))
assert.equal("[1, 2, 3]", stringify([1, 2, 3], {indent: 2}))

var oddball = Object(42)
oddball.__proto__ = { __proto__: null }
assert.equal('{}', stringify(oddball))

/* this WILL throw
var falseNum = Object("37")
falseNum.__proto__ = Number.prototype
assert.equal("{0: '3', 1: '7'}", stringify(falseNum))*/

assert.equal(stringify(Infinity), 'Infinity')
assert.equal(stringify(Infinity, {mode: 'json'}), 'null')
assert.equal(stringify(NaN), 'NaN')
assert.equal(stringify(NaN, {mode: 'json'}), 'null')
assert.equal(stringify(-0), '-0')

assert.equal(stringify('test', null), "'test'")

var array = [""]
var expected = "''"
for (var i = 0; i < 1000; i++) {
  array.push("")
  expected = "''," + expected
}
expected = '[' + expected + ']'
assert.equal(expected, stringify(array, {indent: false}))

assert.strictEqual(stringify([1,2,3], function(){}), undefined)

// don't stringify prototype
assert.equal('{a: 1}', stringify({a:1,__proto__:{b:2}}))

// sort keys tests
assert.equal('{a: 1, b: 2, z: 3}', stringify({b:2,a:1,z:3}, {sort_keys: 1}))
assert.equal('{a: 1, b: {a: 2, b: 5, c: 1}, z: 3}', stringify({b:{c:1,a:2,b:5},a:1,z:3}, {sort_keys: 1}))
assert.equal('{a: [3, 5, 1], b: 2, z: 3}', stringify({b:2,a:[3,5,1],z:3}, {sort_keys: 1}))
assert.equal('{b: 2, a: 1, z: 3}', stringify({b:2,a:1,z:3}, {sort_keys: 0}))
