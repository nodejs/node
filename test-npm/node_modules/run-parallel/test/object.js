var parallel = require('../')
var test = require('tape')

test('functions run in parallel', function (t) {
  t.plan(4)

  var tasks = {
    one: function (cb) {
      t.pass('cb 1')
      cb(null)
    },
    two: function (cb) {
      t.pass('cb 2')
      cb(null)
    },
    three: function (cb) {
      t.pass('cb 3')
      cb(null)
    }
  }

  parallel(tasks, function (err) {
    t.error(err)
  })
})

test('functions that return results', function (t) {
  t.plan(4)

  var tasks = {
    one: function (cb) {
      t.pass('cb 1')
      cb(null, 1)
    },
    two: function (cb) {
      t.pass('cb 2')
      cb(null, 2)
    }
  }

  parallel(tasks, function (err, results) {
    t.error(err)
    t.deepEqual(results, { one: 1, two: 2 })
  })
})
