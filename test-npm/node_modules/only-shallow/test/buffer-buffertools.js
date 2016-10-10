var tap = require('tap')
var test = tap.test
var same = require('../')

test('should match empty Buffers', function (t) {
  t.ok(same(new Buffer([]), new Buffer([])))
  t.end()
})

// remove equals off the list if it's there
test('should match similar Buffers', function (t) {
  var b1 = new Buffer([0])
  b1.equals = null
  var b2 = new Buffer([0])
  b2.equals = null
  t.ok(same(b1, b2))

  var b3 = new Buffer([0, 1, 3])
  b3.equals = null
  var b4 = new Buffer([0, 1, 3])
  b4.equals = null
  t.ok(same(b3, b4))

  t.end()
})

test('should notice different Buffers', function (t) {
  var b1 = new Buffer([0, 1, 2])
  b1.equals = null
  var b2 = new Buffer([0, 1, 23])
  b2.equals = null
  t.notOk(same(b1, b2))

  var shortb = new Buffer([0, 1])
  shortb.equals = null
  var longb = new Buffer(320)
  longb.equals = null
  for (var i = 0; i < 160; i++) longb.writeUInt16LE(i, i * 2)
  t.notOk(same(
    { x: { y: { z: shortb } } },
    { x: { y: { z: longb } } }
  ))
  t.end()
})
