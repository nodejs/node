global.FAKE_WINDOWS = true

var npa = require('../npa.js')
var test = require('tap').test

var cases = {
  'C:\\x\\y\\z': {
    raw: 'C:\\x\\y\\z',
    scope: null,
    name: null,
    escapedName: null,
    rawSpec: 'C:\\x\\y\\z',
    spec: 'C:\\x\\y\\z',
    type: 'local'
  },
  'foo@C:\\x\\y\\z': {
    raw: 'foo@C:\\x\\y\\z',
    scope: null,
    name: 'foo',
    escapedName: 'foo',
    rawSpec: 'C:\\x\\y\\z',
    spec: 'C:\\x\\y\\z',
    type: 'local'
  },
  'foo@file:///C:\\x\\y\\z': {
    raw: 'foo@file:///C:\\x\\y\\z',
    scope: null,
    name: 'foo',
    escapedName: 'foo',
    rawSpec: 'file:///C:\\x\\y\\z',
    spec: 'C:\\x\\y\\z',
    type: 'local'
  },
  'foo@file://C:\\x\\y\\z': {
    raw: 'foo@file://C:\\x\\y\\z',
    scope: null,
    name: 'foo',
    escapedName: 'foo',
    rawSpec: 'file://C:\\x\\y\\z',
    spec: 'C:\\x\\y\\z',
    type: 'local'
  },
  'file:///C:\\x\\y\\z': {
    raw: 'file:///C:\\x\\y\\z',
    scope: null,
    name: null,
    escapedName: null,
    rawSpec: 'file:///C:\\x\\y\\z',
    spec: 'C:\\x\\y\\z',
    type: 'local'
  },
  'file://C:\\x\\y\\z': {
    raw: 'file://C:\\x\\y\\z',
    scope: null,
    name: null,
    escapedName: null,
    rawSpec: 'file://C:\\x\\y\\z',
    spec: 'C:\\x\\y\\z',
    type: 'local'
  },
  'foo@/foo/bar/baz': {
    raw: 'foo@/foo/bar/baz',
    scope: null,
    name: 'foo',
    escapedName: 'foo',
    rawSpec: '/foo/bar/baz',
    spec: '/foo/bar/baz',
    type: 'local'
  }
}

test('parse a windows path', function (t) {
  Object.keys(cases).forEach(function (c) {
    var expect = cases[c]
    var actual = npa(c)
    t.same(actual, expect, c)
  })
  t.end()
})
