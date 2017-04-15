'use strict'
var test = require('tap').test
var requireInject = require('require-inject')
var log = require('npmlog')

/*
The remove actions need to happen in the opposite of their normally defined
order. That is, they need to go shallow -> deep.
*/

var removed = []
var npm = requireInject.installGlobally('../../lib/npm.js', {
  '../../lib/install/action/remove.js': function (staging, pkg, log, next) {
    removed.push(pkg.package.name)
    next()
  }
})

test('setup', function (t) {
  npm.load(function () {
    t.pass('npm loaded')
    t.end()
  })
})

test('abc', function (t) {
  var Installer = require('../../lib/install.js').Installer
  var inst = new Installer(__dirname, false, [])
  inst.progress = {executeActions: log}
  inst.todo = [
    ['remove', {package: {name: 'first'}}],
    ['remove', {package: {name: 'second'}}]
  ]
  inst.executeActions(function () {
    t.isDeeply(removed, ['second', 'first'])
    t.end()
  })
})
