var test = require('tap').test
var init = require('../')
var rimraf = require('rimraf')
var common = require('./lib/common')

test('spaces', function (t) {
  init(__dirname, '', {}, function (er, data) {
    t.ok(!er, 'should not error')
    var wanted = {
      name: 'the-name',
      version: '1.0.0',
      description: '',
      scripts: { test: 'echo "Error: no test specified" && exit 1' },
      license: 'ISC',
      author: '',
      main: 'basic.js'
    }
    t.same(data, wanted)
    t.end()
  })
  common.drive([
    'the name\n',
    'the-name\n',
    '\n',
    '\n',
    '\n',
    '\n',
    '\n',
    '\n',
    '\n',
    '\n',
    'yes\n'
  ])
})

test('teardown', function (t) {
  rimraf(__dirname + '/package.json', t.end.bind(t))
})
