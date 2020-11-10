'use strict'
var test = require('tap').test
var log = require('npmlog')
var npm = require('../../lib/npm.js')

var idealTree = {
  package: {
    name: 'a b c',
    version: '3.what'
  },
  children: [],
  warnings: []
}

test('setup', function (t) {
  npm.load({}, t.end)
})

test('validate-tree', function (t) {
  log.disableProgress()
  var validateTree = require('../../lib/install/validate-tree.js')
  validateTree(idealTree, log.newGroup('validate'), function (er) {
    t.pass("we didn't crash")
    t.end()
  })
})
