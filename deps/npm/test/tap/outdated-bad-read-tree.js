'use strict'
var test = require('tap').test
var requireInject = require('require-inject')
var npm = require('../../lib/npm.js')

test('setup', function (t) {
  npm.load({progress: false}, t.end)
})

test('outdated', function (t) {
  var rptError = new Error('read-package-tree')
  var outdated = requireInject('../../lib/outdated.js', {
    'read-package-tree': function (dir, cb) {
      cb(rptError)
    }
  })
  outdated([], function (err) {
    t.is(err, rptError)
    t.end()
  })
})
