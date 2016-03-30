var common = require('./lib/common')
var init = require('../')
var path = require('path')
var rimraf = require('rimraf')
var test = require('tap').test

test('the basics', function (t) {
  var i = path.join(__dirname, 'basic.input')
  rimraf.sync(__dirname + '/package.json')
  init(__dirname, i, { foo: 'bar' }, function (er, data) {
    if (er) throw er
    var expect = {
      name: 'the-name',
      version: '1.2.5',
      description: 'description',
      author: 'npmbot <n@p.m> (http://npm.im)',
      scripts: { test: 'make test' },
      main: 'main.js',
      config: { foo: 'bar' },
      package: {}
    }
    console.log('')
    t.same(data, expect)
    t.end()
  })
  common.drive([
    'the-name\n',
    'description\n',
    'yes\n'
  ])
})

test('teardown', function (t) {
  rimraf(__dirname + '/package.json', t.end.bind(t))
})
