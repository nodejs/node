var test = require('tap').test
var init = require('../')
var rimraf = require('rimraf')
var common = require('./lib/common')

test('spaces', function (t) {
  rimraf.sync(__dirname + '/package.json')
  init(__dirname, '', {}, function (er, data) {
    if (er)
      throw er
    var wanted = {
      name: 'the-name',
      version: '1.0.0',
      description: '',
      scripts: { test: 'echo "Error: no test specified" && exit 1' },
      license: 'ISC',
      author: '',
      main: 'basic.js'
    }
    console.log('')
    t.has(data, wanted)
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
