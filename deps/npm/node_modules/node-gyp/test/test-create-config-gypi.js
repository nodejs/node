'use strict'

const path = require('path')
const { describe, it } = require('mocha')
const assert = require('assert')
const gyp = require('../lib/node-gyp')
const createConfigGypi = require('../lib/create-config-gypi')
const { parseConfigGypi, getCurrentConfigGypi } = createConfigGypi.test

describe('create-config-gypi', function () {
  it('config.gypi with no options', async function () {
    const prog = gyp()
    prog.parseArgv([])

    const config = await getCurrentConfigGypi({ gyp: prog, vsInfo: {} })
    assert.strictEqual(config.target_defaults.default_configuration, 'Release')
    assert.strictEqual(config.variables.target_arch, process.arch)
  })

  it('config.gypi with --debug', async function () {
    const prog = gyp()
    prog.parseArgv(['_', '_', '--debug'])

    const config = await getCurrentConfigGypi({ gyp: prog, vsInfo: {} })
    assert.strictEqual(config.target_defaults.default_configuration, 'Debug')
  })

  it('config.gypi with custom options', async function () {
    const prog = gyp()
    prog.parseArgv(['_', '_', '--shared-libxml2'])

    const config = await getCurrentConfigGypi({ gyp: prog, vsInfo: {} })
    assert.strictEqual(config.variables.shared_libxml2, true)
  })

  it('config.gypi with nodedir', async function () {
    const nodeDir = path.join(__dirname, 'fixtures', 'nodedir')

    const prog = gyp()
    prog.parseArgv(['_', '_', `--nodedir=${nodeDir}`])

    const config = await getCurrentConfigGypi({ gyp: prog, nodeDir, vsInfo: {} })
    assert.strictEqual(config.variables.build_with_electron, true)
  })

  it('config.gypi with --force-process-config', async function () {
    const nodeDir = path.join(__dirname, 'fixtures', 'nodedir')

    const prog = gyp()
    prog.parseArgv(['_', '_', '--force-process-config', `--nodedir=${nodeDir}`])

    const config = await getCurrentConfigGypi({ gyp: prog, nodeDir, vsInfo: {} })
    assert.strictEqual(config.variables.build_with_electron, undefined)
  })

  it('config.gypi parsing', function () {
    const str = "# Some comments\n{'variables': {'multiline': 'A'\n'B'}}"
    const config = parseConfigGypi(str)
    assert.deepStrictEqual(config, { variables: { multiline: 'AB' } })
  })
})
