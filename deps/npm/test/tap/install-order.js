'use strict'
var test = require('tap').test
var sortActions = require('../../lib/install/diff-trees.js').sortActions
var top = {
  location: '/',
  package: {},
  requiredBy: [],
  requires: [a, b],
  isTop: true
}
var a = {
  location: '/a',
  package: {},
  requiredBy: [],
  requires: [c],
  isTop: false,
  userRequired: false,
  existing: false,
  parent: top
}
var b = {
  location: '/b',
  package: {},
  requiredBy: [],
  requires: [c],
  isTop: false,
  userRequired: false,
  existing: false,
  parent: top
}
var c = {
  location: '/c',
  package: {},
  requiredBy: [a, b],
  requires: [],
  isTop: false,
  userRequired: false,
  existing: false,
  parent: top
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
