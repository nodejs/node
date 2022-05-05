const t = require('tap')
const path = require('path')

const { load: _loadMockNpm } = require('../../fixtures/mock-npm')

// Make less churn in the test to pass in mocks only signature
const loadMockNpm = (t, mocks) => _loadMockNpm(t, { mocks })

t.test('with args, dev=true', async t => {
  const SCRIPTS = []
  let ARB_ARGS = null
  let REIFY_CALLED = false
  let ARB_OBJ = null

  const { npm } = await loadMockNpm(t, {
    '@npmcli/run-script': ({ event }) => {
      SCRIPTS.push(event)
    },
    '@npmcli/arborist': function (args) {
      ARB_ARGS = args
      ARB_OBJ = this
      this.reify = () => {
        REIFY_CALLED = true
      }
    },
    '../../lib/utils/reify-finish.js': (npm, arb) => {
      if (arb !== ARB_OBJ) {
        throw new Error('got wrong object passed to reify-finish')
      }
    },
  })

  // This is here because CI calls tests with `--ignore-scripts`, which config
  // picks up from argv
  npm.config.set('ignore-scripts', false)
  npm.config.set('audit-level', 'low')
  npm.config.set('dev', true)
  // tap sets this to
  // D:\\a\\cli\\cli\\test\\lib\\commands/tap-testdir-install-should-install-globally-using-Arborist
  // which gets normalized elsewhere so comparative tests fail
  npm.prefix = path.resolve(t.testdir({}))

  await npm.exec('install', ['fizzbuzz'])

  t.match(
    ARB_ARGS,
    { global: false, path: npm.prefix, auditLevel: null },
    'Arborist gets correct args and ignores auditLevel'
  )
  t.equal(REIFY_CALLED, true, 'called reify')
  t.strictSame(SCRIPTS, [], 'no scripts when adding dep')
})

t.test('without args', async t => {
  const SCRIPTS = []
  let ARB_ARGS = null
  let REIFY_CALLED = false
  let ARB_OBJ = null

  const { npm } = await loadMockNpm(t, {
    '@npmcli/run-script': ({ event }) => {
      SCRIPTS.push(event)
    },
    '@npmcli/arborist': function (args) {
      ARB_ARGS = args
      ARB_OBJ = this
      this.reify = () => {
        REIFY_CALLED = true
      }
    },
    '../../lib/utils/reify-finish.js': (npm, arb) => {
      if (arb !== ARB_OBJ) {
        throw new Error('got wrong object passed to reify-finish')
      }
    },
  })

  npm.prefix = path.resolve(t.testdir({}))
  npm.config.set('ignore-scripts', false)
  await npm.exec('install', [])
  t.match(ARB_ARGS, { global: false, path: npm.prefix })
  t.equal(REIFY_CALLED, true, 'called reify')
  t.strictSame(SCRIPTS, [
    'preinstall',
    'install',
    'postinstall',
    'prepublish',
    'preprepare',
    'prepare',
    'postprepare',
  ], 'exec scripts when doing local build')
})

t.test('should ignore scripts with --ignore-scripts', async t => {
  const SCRIPTS = []
  let REIFY_CALLED = false
  const { npm } = await loadMockNpm(t, {
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/run-script': ({ event }) => {
      SCRIPTS.push(event)
    },
    '@npmcli/arborist': function () {
      this.reify = () => {
        REIFY_CALLED = true
      }
    },
  })
  npm.config.set('ignore-scripts', true)
  npm.prefix = path.resolve(t.testdir({}))
  await npm.exec('install', [])
  t.equal(REIFY_CALLED, true, 'called reify')
  t.strictSame(SCRIPTS, [], 'no scripts when adding dep')
})

t.test('should install globally using Arborist', async t => {
  const SCRIPTS = []
  let ARB_ARGS = null
  let REIFY_CALLED
  const { npm } = await loadMockNpm(t, {
    '@npmcli/run-script': ({ event }) => {
      SCRIPTS.push(event)
    },
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/arborist': function (args) {
      ARB_ARGS = args
      this.reify = () => {
        REIFY_CALLED = true
      }
    },
  })
  npm.config.set('global', true)
  npm.globalPrefix = path.resolve(t.testdir({}))
  await npm.exec('install', [])
  t.match(
    ARB_ARGS,
    { global: true, path: npm.globalPrefix }
  )
  t.equal(REIFY_CALLED, true, 'called reify')
  t.strictSame(SCRIPTS, [], 'no scripts when installing globally')
})

t.test('should not install invalid global package name', async t => {
  const { npm } = await loadMockNpm(t, {
    '@npmcli/run-script': () => {},
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/arborist': function (args) {
      throw new Error('should not reify')
    },
  })
  npm.config.set('global', true)
  npm.globalPrefix = path.resolve(t.testdir({}))
  await t.rejects(
    npm.exec('install', ['']),
    /Usage:/,
    'should not install invalid package name'
  )
})

t.test('npm i -g npm engines check success', async t => {
  const { npm } = await loadMockNpm(t, {
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/arborist': function () {
      this.reify = () => {}
    },
    pacote: {
      manifest: () => {
        return {
          version: '100.100.100',
          engines: {
            node: '>1',
          },
        }
      },
    },
  })
  npm.globalDir = t.testdir({})
  npm.config.set('global', true)
  await npm.exec('install', ['npm'])
  t.ok('No exceptions happen')
})

t.test('npm i -g npm engines check failure', async t => {
  const { npm } = await loadMockNpm(t, {
    pacote: {
      manifest: () => {
        return {
          _id: 'npm@1.2.3',
          version: '100.100.100',
          engines: {
            node: '>1000',
          },
        }
      },
    },
  })
  npm.globalDir = t.testdir({})
  npm.config.set('global', true)
  await t.rejects(
    npm.exec('install', ['npm']),
    {
      message: 'Unsupported engine',
      pkgid: 'npm@1.2.3',
      current: {
        node: process.version,
        npm: '100.100.100',
      },
      required: {
        node: '>1000',
      },
      code: 'EBADENGINE',
    }
  )
})

t.test('npm i -g npm engines check failure forced override', async t => {
  const { npm } = await loadMockNpm(t, {
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/arborist': function () {
      this.reify = () => {}
    },
    pacote: {
      manifest: () => {
        return {
          _id: 'npm@1.2.3',
          version: '100.100.100',
          engines: {
            node: '>1000',
          },
        }
      },
    },
  })
  npm.globalDir = t.testdir({})
  npm.config.set('global', true)
  npm.config.set('force', true)
  await npm.exec('install', ['npm'])
  t.ok('Does not throw')
})

t.test('npm i -g npm@version engines check failure', async t => {
  const { npm } = await loadMockNpm(t, {
    pacote: {
      manifest: () => {
        return {
          _id: 'npm@1.2.3',
          version: '100.100.100',
          engines: {
            node: '>1000',
          },
        }
      },
    },
  })
  npm.globalDir = t.testdir({})
  npm.config.set('global', true)
  await t.rejects(
    npm.exec('install', ['npm@100']),
    {
      message: 'Unsupported engine',
      pkgid: 'npm@1.2.3',
      current: {
        node: process.version,
        npm: '100.100.100',
      },
      required: {
        node: '>1000',
      },
      code: 'EBADENGINE',
    }
  )
})

t.test('completion', async t => {
  const cwd = process.cwd()
  const testdir = t.testdir({
    arborist: {
      'package.json': '{}',
    },
    'arborist.txt': 'just a file',
    other: {},
  })
  t.afterEach(() => {
    process.chdir(cwd)
  })

  t.test('completion to folder - has a match', async t => {
    const { npm } = await _loadMockNpm(t, { load: false })
    const install = await npm.cmd('install')
    process.chdir(testdir)
    const res = await install.completion({ partialWord: './ar' })
    t.strictSame(res, ['arborist'], 'package dir match')
  })

  t.test('completion to folder - invalid dir', async t => {
    const { npm } = await _loadMockNpm(t, { load: false })
    const install = await npm.cmd('install')
    const res = await install.completion({ partialWord: '/does/not/exist' })
    t.strictSame(res, [], 'invalid dir: no matching')
  })

  t.test('completion to folder - no matches', async t => {
    const { npm } = await _loadMockNpm(t, { load: false })
    const install = await npm.cmd('install')
    process.chdir(testdir)
    const res = await install.completion({ partialWord: './pa' })
    t.strictSame(res, [], 'no name match')
  })

  t.test('completion to folder - match is not a package', async t => {
    const { npm } = await _loadMockNpm(t, { load: false })
    const install = await npm.cmd('install')
    process.chdir(testdir)
    const res = await install.completion({ partialWord: './othe' })
    t.strictSame(res, [], 'no name match')
  })

  t.test('completion to url', async t => {
    const { npm } = await _loadMockNpm(t, { load: false })
    const install = await npm.cmd('install')
    process.chdir(testdir)
    const res = await install.completion({ partialWord: 'http://path/to/url' })
    t.strictSame(res, [])
  })

  t.test('no /', async t => {
    const { npm } = await _loadMockNpm(t, { load: false })
    const install = await npm.cmd('install')
    process.chdir(testdir)
    const res = await install.completion({ partialWord: 'toto' })
    t.notOk(res)
  })

  t.test('only /', async t => {
    const { npm } = await _loadMockNpm(t, { load: false })
    const install = await npm.cmd('install')
    process.chdir(testdir)
    const res = await install.completion({ partialWord: '/' })
    t.strictSame(res, [])
  })
})
