var tap = require('tap')
var test = tap.test
var same = require('../')
var fastEquals = require('buffertools').equals

if (!Buffer.prototype.equals) {
  Buffer.prototype.equals = fastEquals
}

test('should match empty Buffers', function (t) {
  t.ok(same(new Buffer([]), new Buffer([])))
  t.end()
})

test('should match similar Buffers', function (t) {
  var b1 = new Buffer([0])
  var b2 = new Buffer([0])
  t.ok(same(b1, b2))

  var b3 = new Buffer([0, 1, 3])
  var b4 = new Buffer([0, 1, 3])
  t.ok(same(b3, b4))

  t.end()
})

test('should notice different Buffers', function (t) {
  var b1 = new Buffer([0, 1, 2])
  var b2 = new Buffer([0, 1, 23])
  t.notOk(same(b1, b2))

  var shortb = new Buffer([0, 1])
  var longb = new Buffer(320)
  for (var i = 0; i < 160; i++) longb.writeUInt16LE(i, i * 2)
  t.notOk(same(
    { x: { y: { z: shortb } } },
    { x: { y: { z: longb } } }
  ))
  t.end()
})
