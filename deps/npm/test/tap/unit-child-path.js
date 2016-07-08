'use strict'
var test = require('tap').test
var childPath = require('../../lib/utils/child-path.js')
var path = require('path')

test('childPath', function (t) {
  t.is(
    path.resolve(childPath('/path/to', {name: 'abc'})),
    path.resolve('/path/to/node_modules/abc'),
    'basic use')
  t.is(
    path.resolve(childPath('/path/to', {package: {name: '@zed/abc'}})),
    path.resolve('/path/to/node_modules/@zed/abc'),
    'scoped use')
  t.end()
})
