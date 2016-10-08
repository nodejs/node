var tap = require('tap')
tap.test('fast test', function (t) {
  t.test('nested suite', function (t) {
    t.end()
  })
  t.end()
})
