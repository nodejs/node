var tap = require('tap')
var init = require('../')
var rimraf = require('rimraf')

tap.test('the basics', function (t) {
  var i = __dirname + '/basic.input'
  var dir = __dirname
  init(dir, i, {foo:'bar'}, function (er, data) {
    if (er) throw er
    var expect =
      { name: 'the-name',
        version: '1.2.5',
        description: 'description',
        author: 'npmbot <n@p.m> (http://npm.im)',
        scripts: { test: 'make test' },
        main: 'main.js',
        config: { foo: 'bar' },
        package: {} }
    t.same(data, expect)
    t.end()
  })
  setTimeout(function () {
    process.stdin.emit('data', 'the-name\n')
  }, 50)
  setTimeout(function () {
    process.stdin.emit('data', 'description\n')
  }, 100)
  setTimeout(function () {
    process.stdin.emit('data', 'yes\n')
  }, 150)
})

tap.test('teardown', function (t) {
  rimraf(__dirname + '/package.json', t.end.bind(t))
})
