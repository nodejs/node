var tap = require('tap')
var init = require('../')
var rimraf = require('rimraf')
var semver = require('semver')

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
  var stdin = process.stdin
  var name = 'the-name\n'
  var desc = 'description\n'
  var yes = 'yes\n'
  if (semver.gte(process.versions.node, '0.11.0')) {
    ;[name, desc, yes].forEach(function (chunk) {
      stdin.push(chunk)
    })
  } else {
    function input (chunk, ms) {
      setTimeout(function () {
        stdin.emit('data', chunk)
      }, ms)
    }
    stdin.once('readable', function () {
      var ms = 0
      ;[name, desc, yes].forEach(function (chunk) {
        input(chunk, ms += 50)
      })
    })
  }
})

tap.test('teardown', function (t) {
  rimraf(__dirname + '/package.json', t.end.bind(t))
})
