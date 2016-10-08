'use strict'
var test = require('tap').test
var npm = require('../../lib/npm')

test('setup', function (t) {
  npm.load({}, t.done)
})

test('replaceModuleByName', function (t) {
  var replaceModuleByName = require('../../lib/install/deps')._replaceModuleByName
  var mods = []
  for (var ii = 0; ii < 10; ++ii) {
    mods.push({package: {name: ii}, path: '/path/to/' + ii})
  }

  var test = {}
  test.A = mods.slice(0, 4)
  replaceModuleByName(test, 'A', mods[2])
  t.isDeeply(test.A, mods.slice(0, 4), 'replacing an existing module leaves the order alone')
  replaceModuleByName(test, 'A', mods[7])
  t.isDeeply(test.A, mods.slice(0, 4).concat(mods[7]), 'replacing a new module appends')

  test.B = mods.slice(0, 4)
  var replacement = {package: {name: 1}, isReplacement: true}
  replaceModuleByName(test, 'B', replacement)
  t.isDeeply(test.B, [mods[0], replacement, mods[2], mods[3]], 'replacing existing module swaps out for the new version')

  replaceModuleByName(test, 'C', mods[7])
  t.isDeeply(test.C, [mods[7]], 'replacing when the key does not exist yet, causes its creation')

  test.D = mods.slice(0, 4)
  var duplicateByPath = {package: {name: 'dup'}, path: test.D[0].path}
  replaceModuleByName(test, 'D', duplicateByPath)
  t.isDeeply(test.D, mods.slice(0, 4).concat(duplicateByPath), 'replacing with a duplicate path but diff names appends')
  t.end()
})

test('replaceModuleByPath', function (t) {
  var replaceModuleByPath = require('../../lib/install/deps')._replaceModuleByPath
  var mods = []
  for (var ii = 0; ii < 10; ++ii) {
    mods.push({package: {name: ii}, path: '/path/to/' + ii})
  }

  var test = {}
  test.A = mods.slice(0, 4)
  replaceModuleByPath(test, 'A', mods[2])
  t.isDeeply(test.A, mods.slice(0, 4), 'replacing an existing module leaves the order alone')
  replaceModuleByPath(test, 'A', mods[7])
  t.isDeeply(test.A, mods.slice(0, 4).concat(mods[7]), 'replacing a new module appends')

  test.B = mods.slice(0, 4)
  var replacement = {package: {name: 1}, isReplacement: true, path: '/path/to/1'}
  replaceModuleByPath(test, 'B', replacement)
  t.isDeeply(test.B, [mods[0], replacement, mods[2], mods[3]], 'replacing existing module swaps out for the new version')

  replaceModuleByPath(test, 'C', mods[7])
  t.isDeeply(test.C, [mods[7]], 'replacing when the key does not exist yet, causes its creation')

  test.D = mods.slice(0, 4)
  var duplicateByPath = {package: {name: 'dup'}, path: test.D[0].path}
  replaceModuleByPath(test, 'D', duplicateByPath)
  t.isDeeply(test.D, [duplicateByPath].concat(mods.slice(1, 4)), 'replacing with a duplicate path but diff names replaces')
  t.end()
})
