'use strict'

var test = require('tap').test
var install = require('../lib/install').test.install

require('npmlog').level = 'error' // we expect a warning

test('EACCES retry once', function (t) {
  t.plan(3)

  var fs = {}
  fs.stat = function (path, cb) {
    var err = new Error()
    err.code = 'EACCES'
    cb(err)
    t.ok(true);
  }

  var gyp = {}
  gyp.devDir = __dirname
  gyp.opts = {}
  gyp.opts.ensure = true
  gyp.commands = {}
  gyp.commands.install = function (argv, cb) {
    install(fs, gyp, argv, cb)
  }
  gyp.commands.remove = function (argv, cb) {
    cb()
  }

  gyp.commands.install([], function (err) {
    t.ok(true)
    if (/"pre" versions of node cannot be installed/.test(err.message)) {
      t.ok(true)
      t.ok(true)
    }
  })
})
