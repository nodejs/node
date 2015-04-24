
var promzard = require('../')
var test = require('tap').test

test('validate', function (t) {
  t.plan(2)
  var ctx = { tmpdir : '/tmp' }
  var file = __dirname + '/validate.input'
  promzard(file, ctx, function (er, found) {
    t.ok(!er)
    var wanted = { name: 'cool' }
    t.same(found, wanted)
  })
  setTimeout(function () {
    process.stdin.emit('data', 'not cool\n')
  }, 100)
  setTimeout(function () {
    process.stdin.emit('data', 'cool\n')
  }, 200)
})
