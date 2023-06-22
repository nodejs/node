'use strict'

const { describe, it, after } = require('mocha')
const assert = require('assert')
const path = require('path')
const os = require('os')
const util = require('util')
const { test: { download, install } } = require('../lib/install')
const rimraf = require('rimraf')
const gyp = require('../lib/node-gyp')
const log = require('npmlog')
const semver = require('semver')
const stream = require('stream')
const streamPipeline = util.promisify(stream.pipeline)

log.level = 'error' // we expect a warning

describe('install', function () {
  it('EACCES retry once', async () => {
    const fs = {
      promises: {
        stat (_) {
          const err = new Error()
          err.code = 'EACCES'
          assert.ok(true)
          throw err
        }
      }
    }

    const Gyp = {
      devDir: __dirname,
      opts: {
        ensure: true
      },
      commands: {
        install (argv, cb) {
          install(fs, Gyp, argv).then(cb, cb)
        },
        remove (_, cb) {
          cb()
        }
      }
    }

    try {
      await install(fs, Gyp, [])
    } catch (err) {
      assert.ok(true)
      if (/"pre" versions of node cannot be installed/.test(err.message)) {
        assert.ok(true)
      }
    }
  })

  // only run these tests if we are running a version of Node with predictable version path behavior
  const skipParallelInstallTests = process.env.FAST_TEST ||
    process.release.name !== 'node' ||
    semver.prerelease(process.version) !== null ||
    semver.satisfies(process.version, '<10')

  async function parallelInstallsTest (test, fs, devDir, prog) {
    if (skipParallelInstallTests) {
      return test.skip('Skipping parallel installs test due to test environment configuration')
    }

    after(async () => {
      await util.promisify(rimraf)(devDir)
    })

    const expectedDir = path.join(devDir, process.version.replace(/^v/, ''))
    await util.promisify(rimraf)(expectedDir)

    await Promise.all([
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, []),
      install(fs, prog, [])
    ])
  }

  it('parallel installs (ensure=true)', async function () {
    this.timeout(600000)

    const fs = require('graceful-fs')
    const devDir = await util.promisify(fs.mkdtemp)(path.join(os.tmpdir(), 'node-gyp-test-'))

    const prog = gyp()
    prog.parseArgv([])
    prog.devDir = devDir
    prog.opts.ensure = true
    log.level = 'warn'

    await parallelInstallsTest(this, fs, devDir, prog)
  })

  it('parallel installs (ensure=false)', async function () {
    this.timeout(600000)

    const fs = require('graceful-fs')
    const devDir = await util.promisify(fs.mkdtemp)(path.join(os.tmpdir(), 'node-gyp-test-'))

    const prog = gyp()
    prog.parseArgv([])
    prog.devDir = devDir
    prog.opts.ensure = false
    log.level = 'warn'

    await parallelInstallsTest(this, fs, devDir, prog)
  })

  it('parallel installs (tarball)', async function () {
    this.timeout(600000)

    const fs = require('graceful-fs')
    const devDir = await util.promisify(fs.mkdtemp)(path.join(os.tmpdir(), 'node-gyp-test-'))

    const prog = gyp()
    prog.parseArgv([])
    prog.devDir = devDir
    prog.opts.tarball = path.join(devDir, 'node-headers.tar.gz')
    log.level = 'warn'

    await streamPipeline(
      (await download(prog, `https://nodejs.org/dist/${process.version}/node-${process.version}.tar.gz`)).body,
      fs.createWriteStream(prog.opts.tarball)
    )

    await parallelInstallsTest(this, fs, devDir, prog)
  })
})
