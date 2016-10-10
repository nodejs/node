var parallel = require('../')
var test = require('tape')

test('functions that return errors (array)', function (t) {
  t.plan(3)

  var tasks = [
    function (cb) {
      t.pass('cb 1')
      cb(new Error('oops'))
    },
    function (cb) {
      setTimeout(function () {
        t.pass('cb 2')
        cb(null, 2)
      }, 100)
    }
  ]

  parallel(tasks, function (err) {
    t.ok(err instanceof Error)
  })
})

test('functions that return errors (object)', function (t) {
  t.plan(3)

  var tasks = {
    one: function (cb) {
      t.pass('cb 1')
      cb(new Error('oops'))
    },
    two: function (cb) {
      setTimeout(function () {
        t.pass('cb 2')
        cb(null, 2)
      }, 100)
    }
  }

  parallel(tasks, function (err) {
    t.ok(err instanceof Error)
  })
})

test('functions that return errors (object) w/ partial results', function (t) {
  t.plan(4)

  var tasks = {
    one: function (cb) {
      t.pass('cb 1')
      cb(null, 1)
    },
    two: function (cb) {
      setTimeout(function () {
        t.pass('cb 2')
        cb(new Error('oops'))
      }, 100)
    }
  }

  parallel(tasks, function (err, results) {
    t.ok(err instanceof Error)
    t.deepEqual(results, { one: 1, two: undefined })
  })
})

test('array of functions that produce multiple errors', function (t) {
  var tasks = [
    function (cb) {
      cb(new Error('a'))
    },
    function (cb) {
      cb(new Error('b'))
    }
  ]
  parallel(tasks, function (err) {
    t.ok(err.message === 'a', 'only observes the first error')
    t.end()
  })
})
