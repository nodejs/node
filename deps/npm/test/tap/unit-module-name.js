'use strict'
var test = require('tap').test
var moduleName = require('../../lib/utils/module-name.js')

test('pathToPackageName', function (t) {
  var pathToPackageName = moduleName.test.pathToPackageName
  t.is(pathToPackageName('/foo/bar/baz/bark'), 'bark', 'simple module name')
  t.is(pathToPackageName('/foo/bar/@baz/bark'), '@baz/bark', 'scoped module name')
  t.is(pathToPackageName('/foo'), 'foo', 'module at top')
  t.is(pathToPackageName('/@foo'), '@foo', 'invalid module at top')
  t.is(pathToPackageName('/'), '', 'root, empty result')
  t.is(pathToPackageName(''), '', 'empty, empty')
  t.is(pathToPackageName(undefined), '', 'undefined is empty')
  t.is(pathToPackageName(null), '', 'null is empty')
  t.done()
})

test('isNotEmpty', function (t) {
  var isNotEmpty = moduleName.test.isNotEmpty
  t.is(isNotEmpty('abc'), true, 'string is not empty')
  t.is(isNotEmpty(''), false, 'empty string is empty')
  t.is(isNotEmpty(null), false, 'null is empty')
  t.is(isNotEmpty(undefined), false, 'undefined is empty')
  t.is(isNotEmpty(0), true, 'zero is not empty')
  t.is(isNotEmpty(true), true, 'true is not empty')
  t.is(isNotEmpty([]), true, 'empty array is not empty')
  t.is(isNotEmpty({}), true, 'object is not empty')
  t.done()
})

test('moduleName', function (t) {
  t.is(moduleName({package: {name: 'foo'}}), 'foo', 'package named')
  t.is(moduleName({name: 'foo'}), 'foo', 'package named, no tree')
  t.is(moduleName({path: '/foo/bar'}), 'bar', 'path named')
  t.is(moduleName({}), '!invalid#1', 'no named')
  t.is(moduleName({path: '/'}), '!invalid#2', 'invalid named')
  var obj = {}
  t.is(moduleName(obj), moduleName(obj), 'once computed, an invalid module name will not change')
  t.done()
})
