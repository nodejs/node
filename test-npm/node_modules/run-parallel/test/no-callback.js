var parallel = require('../')
var test = require('tape')

test('no callback (array)', function (t) {
  t.plan(2)

  var tasks = [
    function (cb) {
      t.pass('cb 1')
    },
    function (cb) {
      t.pass('cb 2')
    }
  ]

  parallel(tasks)
})

test('no callback (object)', function (t) {
  t.plan(2)

  var tasks = {
    one: function (cb) {
      t.pass('cb 1')
    },
    two: function (cb) {
      t.pass('cb 2')
    }
  }

  parallel(tasks)
})
