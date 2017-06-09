var tap = require('tap')
var init = require('../')
var rimraf = require('rimraf')

var log = console.log
var logged = false
console.log = function () {
  logged = true
}

tap.test('silent: true', function (t) {
  init(__dirname, __dirname, {yes: 'yes', silent: true}, function (er, data) {
    if (er) throw er

    t.false(logged, 'did not print anything')
    t.end()
  })
})

tap.test('teardown', function (t) {
  console.log = log
  rimraf(__dirname + '/package.json', t.end.bind(t))
})
