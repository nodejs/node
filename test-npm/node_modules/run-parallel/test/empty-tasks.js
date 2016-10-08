var parallel = require('../')
var test = require('tape')

test('empty tasks array', function (t) {
  t.plan(1)

  parallel([], function (err) {
    t.error(err)
  })
})

test('empty tasks object', function (t) {
  t.plan(1)

  parallel({}, function (err) {
    t.error(err)
  })
})

test('empty tasks array and no callback', function (t) {
  parallel([])
  t.pass('did not throw')
  t.end()
})

test('empty tasks object and no callback', function (t) {
  parallel({})
  t.pass('did not throw')
  t.end()
})
