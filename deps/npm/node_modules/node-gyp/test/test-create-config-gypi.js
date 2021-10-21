'use strict'

const { test } = require('tap')
const gyp = require('../lib/node-gyp')
const createConfigGypi = require('../lib/create-config-gypi')
const { getCurrentConfigGypi } = createConfigGypi.test

test('config.gypi with no options', function (t) {
  t.plan(2)

  const prog = gyp()
  prog.parseArgv([])

  const config = getCurrentConfigGypi({ gyp: prog, vsInfo: {} })
  t.equal(config.target_defaults.default_configuration, 'Release')
  t.equal(config.variables.target_arch, process.arch)
})

test('config.gypi with --debug', function (t) {
  t.plan(1)

  const prog = gyp()
  prog.parseArgv(['_', '_', '--debug'])

  const config = getCurrentConfigGypi({ gyp: prog, vsInfo: {} })
  t.equal(config.target_defaults.default_configuration, 'Debug')
})

test('config.gypi with custom options', function (t) {
  t.plan(1)

  const prog = gyp()
  prog.parseArgv(['_', '_', '--shared-libxml2'])

  const config = getCurrentConfigGypi({ gyp: prog, vsInfo: {} })
  t.equal(config.variables.shared_libxml2, true)
})
