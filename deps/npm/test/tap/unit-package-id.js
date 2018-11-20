'use strict'
var test = require('tap').test
var packageId = require('../../lib/utils/package-id.js')

test('packageId', function (t) {
  t.is(packageId({package: {_id: 'abc@123'}}), 'abc@123', 'basic')
  t.is(packageId({_id: 'abc@123'}), 'abc@123', 'basic no tree')
  t.is(packageId({package: {name: 'abc', version: '123'}}), 'abc@123', 'computed')
  t.is(packageId({package: {_id: '@', name: 'abc', version: '123'}}), 'abc@123', 'computed, ignore invalid id')
  t.is(packageId({package: {name: 'abc'}}), 'abc', 'no version')
  t.is(packageId({package: {version: '123'}}), '!invalid#1@123', 'version, no name')
  t.is(packageId({package: {version: '123'}, path: '/path/to/abc'}), 'abc@123', 'version path-name')
  t.is(packageId({package: {version: '123'}, path: '/path/@to/abc'}), '@to/abc@123', 'version scoped-path-name')
  t.is(packageId({path: '/path/to/abc'}), 'abc', 'path name, no version')
  t.is(packageId({}), '!invalid#2', 'nothing')
  t.done()
})
