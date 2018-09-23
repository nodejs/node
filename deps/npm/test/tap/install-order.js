'use strict'
var test = require('tap').test
var sortActions = require('../../lib/install/diff-trees.js').sortActions

var a = {
  package: {_location: '/a', _requiredBy: []}
}
var b = {
  package: {_location: '/b', _requiredBy: []}
}
var c = {
  package: {_location: '/c', _requiredBy: ['/a', '/b']}
}

test('install-order when installing deps', function (t) {
  var plain = [
    ['add', a],
    ['add', b],
    ['add', c]]
  var sorted = [
    ['add', c],
    ['add', a],
    ['add', b]]
  t.isDeeply(sortActions(plain), sorted)
  t.end()
})

test('install-order when not installing deps', function (t) {
  var plain = [
    ['add', a],
    ['add', b]]
  var sorted = [
    ['add', a],
    ['add', b]]
  t.isDeeply(sortActions(plain), sorted)
  t.end()
})
