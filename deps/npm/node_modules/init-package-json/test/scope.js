var tap = require('tap')
var init = require('../')
var rimraf = require('rimraf')

tap.test('the scope', function (t) {
  var i = __dirname + '/basic.input'
  var dir = __dirname
  init(dir, i, {scope: 'foo'}, function (er, data) {
    if (er) throw er
    var expect =
      { name: '@foo/test',
        version: '1.2.5',
        description: 'description',
        author: 'me <em@i.l> (http://url)',
        scripts: { test: 'make test' },
        main: 'main.js',
        config: { scope: 'foo' },
        package: {} }
    t.same(data, expect)
    t.end()
  })
  setTimeout(function () {
    process.stdin.emit('data', '@foo/test\n')
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
