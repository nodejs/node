'use strict'
var test = require('tap').test
var npm = require('../../lib/npm')

test('setup', function (t) {
  npm.load({}, t.done)
})

test('replaceModule', function (t) {
  var replaceModule = require('../../lib/install/deps')._replaceModule
  var mods = []
  for (var ii = 0; ii < 10; ++ii) {
    mods.push({package: {name: ii}})
  }

  var test = {}
  test.A = mods.slice(0, 4)
  replaceModule(test, 'A', mods[2])
  t.isDeeply(test.A, mods.slice(0, 4), 'replacing an existing module leaves the order alone')
  replaceModule(test, 'A', mods[7])
  t.isDeeply(test.A, mods.slice(0, 4).concat(mods[7]), 'replacing a new module appends')

  test.B = mods.slice(0, 4)
  var replacement = {package: {name: 1}, isReplacement: true}
  replaceModule(test, 'B', replacement)
  t.isDeeply(test.B, [mods[0], replacement, mods[2], mods[3]], 'replacing existing module swaps out for the new version')

  replaceModule(test, 'C', mods[7])
  t.isDeeply(test.C, [mods[7]], 'replacing when the key does not exist yet, causes its creation')
  t.end()
})

test('replaceModuleName', function (t) {
  var replaceModuleName = require('../../lib/install/deps')._replaceModuleName
  var mods = []
  for (var ii = 0; ii < 10; ++ii) {
    mods.push('pkg' + ii)
  }

  var test = {}
  test.A = mods.slice(0, 4)
  replaceModuleName(test, 'A', mods[2])
  t.isDeeply(test.A, mods.slice(0, 4), 'replacing an existing module leaves the order alone')
  replaceModuleName(test, 'A', mods[7])
  t.isDeeply(test.A, mods.slice(0, 4).concat(mods[7]), 'replacing a new module appends')

  replaceModuleName(test, 'C', mods[7])
  t.isDeeply(test.C, [mods[7]], 'replacing when the key does not exist yet, causes its creation')
  t.end()
})
