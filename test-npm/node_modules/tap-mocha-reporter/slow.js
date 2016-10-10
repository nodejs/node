var tap = require('tap')
tap.test('slow test', function (t) {
  setTimeout(function () {
    t.end()
  }, 5000)
})
