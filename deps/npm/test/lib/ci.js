const fs = require('fs')
const util = require('util')
const readdir = util.promisify(fs.readdir)

const { test } = require('tap')

const requireInject = require('require-inject')

test('should ignore scripts with --ignore-scripts', (t) => {
  const SCRIPTS = []
  let REIFY_CALLED = false
  const ci = requireInject('../../lib/ci.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    '../../lib/npm.js': {
      globalDir: 'path/to/node_modules/',
      prefix: 'foo',
      flatOptions: {
        global: false,
        ignoreScripts: true,
      },
      config: {
        get: () => false,
      },
    },
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
  ci([], er => {
    if (er)
      throw er
    t.equal(REIFY_CALLED, true, 'called reify')
    t.strictSame(SCRIPTS, [], 'no scripts when running ci')
    t.end()
  })
})

test('should use Arborist and run-script', (t) => {
  const scripts = [
    'preinstall',
    'install',
    'postinstall',
    'prepublish', // XXX should we remove this finally??
    'preprepare',
    'prepare',
    'postprepare',
  ]
  const ci = requireInject('../../lib/ci.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        global: false,
      },
    },
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
    util: {
      inherits: () => {},
      promisify: (fn) => fn,
    },
    rimraf: (path) => {
      t.ok(path, 'rimraf called with path')
      return Promise.resolve(true)
    },
    '../../lib/utils/reify-output.js': function (arb) {
      t.ok(arb, 'gets arborist tree')
    },
  })
  ci(null, er => {
    if (er)
      throw er
    t.strictSame(scripts, [], 'called all scripts')
    t.end()
  })
})

test('should pass flatOptions to Arborist.reify', (t) => {
  const ci = requireInject('../../lib/ci.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        production: true,
      },
    },
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
  ci(null, er => {
    if (er)
      throw er
  })
})

test('should throw if package-lock.json or npm-shrinkwrap missing', (t) => {
  const testDir = t.testdir({
    'index.js': 'some contents',
    'package.json': 'some info',
  })

  const ci = requireInject('../../lib/ci.js', {
    '../../lib/npm.js': {
      prefix: testDir,
      flatOptions: {
        global: false,
      },
    },
    '@npmcli/run-script': opts => {},
    '../../lib/utils/reify-finish.js': async () => {},
    npmlog: {
      verbose: () => {
        t.ok(true, 'log fn called')
      },
    },
  })
  ci(null, (err, res) => {
    t.ok(err, 'throws error when there is no package-lock')
    t.notOk(res)
    t.end()
  })
})

test('should throw ECIGLOBAL', (t) => {
  const ci = requireInject('../../lib/ci.js', {
    '../../lib/npm.js': {
      prefix: 'foo',
      flatOptions: {
        global: true,
      },
    },
    '@npmcli/run-script': opts => {},
    '../../lib/utils/reify-finish.js': async () => {},
  })
  ci(null, (err, res) => {
    t.equals(err.code, 'ECIGLOBAL', 'throws error with global packages')
    t.notOk(res)
    t.end()
  })
})

test('should remove existing node_modules before installing', (t) => {
  const testDir = t.testdir({
    node_modules: {
      'some-file': 'some contents',
    },
  })

  const ci = requireInject('../../lib/ci.js', {
    '../../lib/npm.js': {
      prefix: testDir,
      flatOptions: {
        global: false,
      },
    },
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

  ci(null, er => {
    if (er)
      throw er
  })
})
