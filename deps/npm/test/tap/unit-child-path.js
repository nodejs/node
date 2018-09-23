'use strict'
var test = require('tap').test
var childPath = require('../../lib/utils/child-path.js')

test('childPath', function (t) {
  t.is(childPath('/path/to', {name: 'abc'}), '/path/to/node_modules/abc', 'basic use')
  t.is(childPath('/path/to', {package: {name: '@zed/abc'}}), '/path/to/node_modules/@zed/abc', 'scoped use')
  t.end()
})
