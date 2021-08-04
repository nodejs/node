const fs = require('fs')
const util = require('util')
const readdir = util.promisify(fs.readdir)

const t = require('tap')

const { fake: mockNpm } = require('../fixtures/mock-npm')

t.test('should ignore scripts with --ignore-scripts', (t) => {
  const SCRIPTS = []
  let REIFY_CALLED = false
  const CI = t.mock('../../lib/ci.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/run-script': ({ event }) => {
      SCRIPTS.push(event)
    },
    '@npmcli/arborist': function () {
      this.loadVirtual = async () => {}
      this.reify = () => {
        REIFY_CALLED = true
      }
    },
  })

  const npm = mockNpm({
    globalDir: 'path/to/node_modules/',
    prefix: 'foo',
    config: {
      global: false,
      'ignore-scripts': true,
    },
  })
  const ci = new CI(npm)

  ci.exec([], er => {
    if (er)
      throw er
    t.equal(REIFY_CALLED, true, 'called reify')
    t.strictSame(SCRIPTS, [], 'no scripts when running ci')
    t.end()
  })
})

t.test('should use Arborist and run-script', (t) => {
  const scripts = [
    'preinstall',
    'install',
    'postinstall',
    'prepublish', // XXX should we remove this finally??
    'preprepare',
    'prepare',
    'postprepare',
  ]

  // set to true when timer starts, false when it ends
  // when the test is done, we assert that all timers ended
  const timers = {}
  const onTime = msg => {
    if (timers[msg])
      throw new Error(`saw duplicate timer: ${msg}`)
    timers[msg] = true
  }
  const onTimeEnd = msg => {
    if (!timers[msg])
      throw new Error(`ended timer that was not started: ${msg}`)
    timers[msg] = false
  }
  process.on('time', onTime)
  process.on('timeEnd', onTimeEnd)
  t.teardown(() => {
    process.removeListener('time', onTime)
    process.removeListener('timeEnd', onTimeEnd)
  })

  const path = t.testdir({
    node_modules: {
      foo: {
        'package.json': JSON.stringify({
          name: 'foo',
          version: '1.2.3',
        }),
      },
      '.dotdir': {},
      '.dotfile': 'a file with a dot',
    },
  })
  const expectRimrafs = 3
  let actualRimrafs = 0

  const CI = t.mock('../../lib/ci.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/run-script': opts => {
      t.match(opts, { event: scripts.shift() })
    },
    '@npmcli/arborist': function (args) {
      t.ok(args, 'gets options object')
      this.loadVirtual = () => {
        t.ok(true, 'loadVirtual is called')
        return Promise.resolve(true)
      }
      this.reify = () => {
        t.ok(true, 'reify is called')
      }
    },
    rimraf: (path, ...args) => {
      actualRimrafs++
      t.ok(path, 'rimraf called with path')
      // callback is always last arg
      args.pop()()
    },
    '../../lib/utils/reify-output.js': function (arb) {
      t.ok(arb, 'gets arborist tree')
    },
  })

  const npm = mockNpm({
    prefix: path,
    config: {
      global: false,
    },
  })
  const ci = new CI(npm)

  ci.exec(null, er => {
    if (er)
      throw er
    for (const [msg, result] of Object.entries(timers))
      t.notOk(result, `properly resolved ${msg} timer`)
    t.match(timers, { 'npm-ci:rm': false }, 'saw the rimraf timer')
    t.equal(actualRimrafs, expectRimrafs, 'removed the right number of things')
    t.strictSame(scripts, [], 'called all scripts')
    t.end()
  })
})

t.test('should pass flatOptions to Arborist.reify', (t) => {
  const CI = t.mock('../../lib/ci.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/run-script': opts => {},
    '@npmcli/arborist': function () {
      this.loadVirtual = () => Promise.resolve(true)
      this.reify = async (options) => {
        t.equal(options.production, true, 'should pass flatOptions to Arborist.reify')
        t.end()
      }
    },
  })
  const npm = mockNpm({
    prefix: 'foo',
    flatOptions: {
      production: true,
    },
  })
  const ci = new CI(npm)
  ci.exec(null, er => {
    if (er)
      throw er
  })
})

t.test('should throw if package-lock.json or npm-shrinkwrap missing', (t) => {
  const testDir = t.testdir({
    'index.js': 'some contents',
    'package.json': 'some info',
  })

  const CI = t.mock('../../lib/ci.js', {
    '@npmcli/run-script': opts => {},
    '../../lib/utils/reify-finish.js': async () => {},
    npmlog: {
      verbose: () => {
        t.ok(true, 'log fn called')
      },
    },
  })
  const npm = mockNpm({
    prefix: testDir,
    config: {
      global: false,
    },
  })
  const ci = new CI(npm)
  ci.exec(null, (err, res) => {
    t.match(err, /package-lock.json/, 'throws error when there is no package-lock')
    t.notOk(res)
    t.end()
  })
})

t.test('should throw ECIGLOBAL', (t) => {
  const CI = t.mock('../../lib/ci.js', {
    '@npmcli/run-script': opts => {},
    '../../lib/utils/reify-finish.js': async () => {},
  })
  const npm = mockNpm({
    prefix: 'foo',
    config: {
      global: true,
    },
  })
  const ci = new CI(npm)
  ci.exec(null, (err, res) => {
    t.equal(err.code, 'ECIGLOBAL', 'throws error with global packages')
    t.notOk(res)
    t.end()
  })
})

t.test('should remove existing node_modules before installing', (t) => {
  const testDir = t.testdir({
    node_modules: {
      'some-file': 'some contents',
    },
  })

  const CI = t.mock('../../lib/ci.js', {
    '@npmcli/run-script': opts => {},
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/arborist': function () {
      this.loadVirtual = () => Promise.resolve(true)
      this.reify = async (options) => {
        t.equal(options.save, false, 'npm ci should never save')
        // check if node_modules was removed before reifying
        const contents = await readdir(testDir)
        const nodeModules = contents.filter((path) => path.startsWith('node_modules'))
        t.same(nodeModules, ['node_modules'], 'should only have the node_modules directory')
        t.end()
      }
    },
  })

  const npm = mockNpm({
    prefix: testDir,
    config: {
      global: false,
    },
  })
  const ci = new CI(npm)

  ci.exec(null, er => {
    if (er)
      throw er
  })
})
