var Readable = require('stream').Readable
var tape = require('tape')
var union = require('./')

tape('numbers', function (t) {
  var a = new Readable({objectMode: true})
  var b = new Readable({objectMode: true})
  a._read = b._read = function () {}

  a.push(4)
  a.push(6)
  a.push(10)
  a.push(14)
  a.push(15)
  a.push(20)
  a.push(22)
  a.push(null)

  b.push(6)
  b.push(11)
  b.push(20)
  b.push(null)

  var u = union(a, b)
  var expected = [4, 6, 10, 11, 14, 15, 20, 22]

  u.on('data', function (data) {
    t.same(data, expected.shift())
  })

  u.on('end', function () {
    t.same(expected.length, 0, 'no more data')
    t.end()
  })
})

tape('string', function (t) {
  var a = new Readable({objectMode: true})
  var b = new Readable({objectMode: true})
  a._read = b._read = function () {}

  a.push('04')
  a.push('06')
  a.push('10')
  a.push('14')
  a.push('15')
  a.push('20')
  a.push('22')
  a.push(null)

  b.push('06')
  b.push('11')
  b.push('20')
  b.push(null)

  var u = union(a, b)
  var expected = ['04', '06', '10', '11', '14', '15', '20', '22']

  u.on('data', function (data) {
    t.same(data, expected.shift())
  })

  u.on('end', function () {
    t.same(expected.length, 0, 'no more data')
    t.end()
  })
})

tape('objects', function (t) {
  var a = new Readable({objectMode: true})
  var b = new Readable({objectMode: true})
  a._read = b._read = function () {}

  a.push({key: '04'})
  a.push({key: '06'})
  a.push({key: '10'})
  a.push({key: '14'})
  a.push({key: '15'})
  a.push({key: '20'})
  a.push({key: '22'})
  a.push(null)

  b.push({key: '06'})
  b.push({key: '11'})
  b.push({key: '20'})
  b.push(null)

  var u = union(a, b)
  var expected = [{key: '04'}, {key: '06'}, {key: '10'}, {key: '11'}, {key: '14'}, {key: '15'}, {key: '20'}, {key: '22'}]

  u.on('data', function (data) {
    t.same(data, expected.shift())
  })

  u.on('end', function () {
    t.same(expected.length, 0, 'no more data')
    t.end()
  })
})

tape('custom objects', function (t) {
  var a = new Readable({objectMode: true})
  var b = new Readable({objectMode: true})
  a._read = b._read = function () {}

  a.push({bar: '04'})
  a.push({bar: '06'})
  a.push({bar: '10'})
  a.push({bar: '14'})
  a.push({bar: '15'})
  a.push({bar: '20'})
  a.push({bar: '22'})
  a.push(null)

  b.push({bar: '06'})
  b.push({bar: '11'})
  b.push({bar: '20'})
  b.push(null)

  var u = union(a, b, function (data) {
    return data.bar
  })

  var expected = [{bar: '04'}, {bar: '06'}, {bar: '10'}, {bar: '11'}, {bar: '14'}, {bar: '15'}, {bar: '20'}, {bar: '22'}]

  u.on('data', function (data) {
    t.same(data, expected.shift())
  })

  u.on('end', function () {
    t.same(expected.length, 0, 'no more data')
    t.end()
  })
})

tape('destroy stream', function (t) {
  var a = new Readable({objectMode: true})
  var b = new Readable({objectMode: true})

  a._read = function () {}
  b._read = function () {}

  t.plan(2)

  a.destroy = function () {
    t.ok(true)
  }

  b.destroy = function () {
    t.ok(true)
  }

  union(a, b).destroy()
})
