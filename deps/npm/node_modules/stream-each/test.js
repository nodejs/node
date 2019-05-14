var tape = require('tape')
var through = require('through2')
var each = require('./')

tape('each', function (t) {
  var s = through.obj()
  s.write('a')
  s.write('b')
  s.write('c')
  s.end()

  s.on('end', function () {
    t.end()
  })

  var expected = ['a', 'b', 'c']
  each(s, function (data, next) {
    t.same(data, expected.shift())
    next()
  })
})

tape('each and callback', function (t) {
  var s = through.obj()
  s.write('a')
  s.write('b')
  s.write('c')
  s.end()

  var expected = ['a', 'b', 'c']
  each(s, function (data, next) {
    t.same(data, expected.shift())
    next()
  }, function () {
    t.end()
  })
})

tape('each (write after)', function (t) {
  var s = through.obj()
  s.on('end', function () {
    t.end()
  })

  var expected = ['a', 'b', 'c']
  each(s, function (data, next) {
    t.same(data, expected.shift())
    next()
  })

  setTimeout(function () {
    s.write('a')
    s.write('b')
    s.write('c')
    s.end()
  }, 100)
})

tape('each error', function (t) {
  var s = through.obj()
  s.write('hello')
  s.on('error', function (err) {
    t.same(err.message, 'stop')
    t.end()
  })

  each(s, function (data, next) {
    next(new Error('stop'))
  })
})

tape('each error and callback', function (t) {
  var s = through.obj()
  s.write('hello')

  each(s, function (data, next) {
    next(new Error('stop'))
  }, function (err) {
    t.same(err.message, 'stop')
    t.end()
  })
})

tape('each with falsey values', function (t) {
  var s = through.obj()
  s.write(0)
  s.write(false)
  s.write(undefined)
  s.end()

  s.on('end', function () {
    t.end()
  })

  var expected = [0, false]
  var count = 0
  each(s, function (data, next) {
    count++
    t.same(data, expected.shift())
    next()
  }, function () {
    t.same(count, 2)
  })
})

tape('huge stack', function (t) {
  var s = through.obj()

  for (var i = 0; i < 5000; i++) {
    s.write('foo')
  }

  s.end()

  each(s, function (data, cb) {
    if (data !== 'foo') t.fail('bad data')
    cb()
  }, function (err) {
    t.error(err, 'no error')
    t.end()
  })
})
