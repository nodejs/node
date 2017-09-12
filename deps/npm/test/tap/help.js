var test = require('tap').test
var common = require('../common-tap')

test('npm food', function (t) {
  common.npm('food', {}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.equal(code, 1, 'command ran with error')

    t.has(stdout, 'Did you mean this?')

    t.notOk(stderr, 'stderr should be empty')
    t.end()
  })
})

test('npm jet', function (t) {
  common.npm('jet', {}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.equal(code, 1, 'command ran with error')

    t.has(stdout, 'Did you mean one of these?')

    t.notOk(stderr, 'stderr should be empty')
    t.end()
  })
})
