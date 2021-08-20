const t = require('tap')

const Install = require('../../lib/install.js')
const { fake: mockNpm } = require('../fixtures/mock-npm')

t.test('should install using Arborist', (t) => {
  const SCRIPTS = []
  let ARB_ARGS = null
  let REIFY_CALLED = false
  let ARB_OBJ = null

  const Install = t.mock('../../lib/install.js', {
    '@npmcli/run-script': ({ event }) => {
      SCRIPTS.push(event)
    },
    npmlog: {
      warn: () => {},
    },
    '@npmcli/arborist': function (args) {
      ARB_ARGS = args
      ARB_OBJ = this
      this.reify = () => {
        REIFY_CALLED = true
      }
    },
    '../../lib/utils/reify-finish.js': (npm, arb) => {
      if (arb !== ARB_OBJ)
        throw new Error('got wrong object passed to reify-finish')
    },
  })

  const npm = mockNpm({
    config: { dev: true },
    flatOptions: { global: false, auditLevel: 'low' },
    globalDir: 'path/to/node_modules/',
    prefix: 'foo',
  })
  const install = new Install(npm)

  t.test('with args', t => {
    install.exec(['fizzbuzz'], er => {
      if (er)
        throw er
      t.match(ARB_ARGS,
        { global: false, path: 'foo', auditLevel: null },
        'Arborist gets correct args and ignores auditLevel')
      t.equal(REIFY_CALLED, true, 'called reify')
      t.strictSame(SCRIPTS, [], 'no scripts when adding dep')
      t.end()
    })
  })

  t.test('just a local npm install', t => {
    install.exec([], er => {
      if (er)
        throw er
      t.match(ARB_ARGS, { global: false, path: 'foo' })
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
      t.end()
    })
  })

  t.end()
})

t.test('should ignore scripts with --ignore-scripts', (t) => {
  const SCRIPTS = []
  let REIFY_CALLED = false
  const Install = t.mock('../../lib/install.js', {
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
  const npm = mockNpm({
    globalDir: 'path/to/node_modules/',
    prefix: 'foo',
    flatOptions: { global: false },
    config: {
      global: false,
      'ignore-scripts': true,
    },
  })
  const install = new Install(npm)
  install.exec([], er => {
    if (er)
      throw er
    t.equal(REIFY_CALLED, true, 'called reify')
    t.strictSame(SCRIPTS, [], 'no scripts when adding dep')
    t.end()
  })
})

t.test('should install globally using Arborist', (t) => {
  const Install = t.mock('../../lib/install.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    '@npmcli/arborist': function () {
      this.reify = () => {}
    },
  })
  const npm = mockNpm({
    globalDir: 'path/to/node_modules/',
    prefix: 'foo',
    config: { global: true },
    flatOptions: { global: true },
  })
  const install = new Install(npm)
  install.exec([], er => {
    if (er)
      throw er
    t.end()
  })
})

t.test('completion to folder', async t => {
  const Install = t.mock('../../lib/install.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    util: {
      promisify: (fn) => fn,
    },
    fs: {
      readdir: (path) => {
        if (path === '/')
          return ['arborist']
        else
          return ['package.json']
      },
    },
  })
  const install = new Install({})
  const res = await install.completion({ partialWord: '/ar' })
  const expect = process.platform === 'win32' ? '\\arborist' : '/arborist'
  t.strictSame(res, [expect], 'package dir match')
  t.end()
})

t.test('completion to folder - invalid dir', async t => {
  const Install = t.mock('../../lib/install.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    util: {
      promisify: (fn) => fn,
    },
    fs: {
      readdir: () => {
        throw new Error('EONT')
      },
    },
  })
  const install = new Install({})
  const res = await install.completion({ partialWord: 'path/to/folder' })
  t.strictSame(res, [], 'invalid dir: no matching')
  t.end()
})

t.test('completion to folder - no matches', async t => {
  const Install = t.mock('../../lib/install.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    util: {
      promisify: (fn) => fn,
    },
    fs: {
      readdir: (path) => {
        return ['foobar']
      },
    },
  })
  const install = new Install({})
  const res = await install.completion({ partialWord: '/pa' })
  t.strictSame(res, [], 'no name match')
  t.end()
})

t.test('completion to folder - match is not a package', async t => {
  const Install = t.mock('../../lib/install.js', {
    '../../lib/utils/reify-finish.js': async () => {},
    util: {
      promisify: (fn) => fn,
    },
    fs: {
      readdir: (path) => {
        if (path === '/')
          return ['arborist']
        else
          throw new Error('EONT')
      },
    },
  })
  const install = new Install({})
  const res = await install.completion({ partialWord: '/ar' })
  t.strictSame(res, [], 'no name match')
  t.end()
})

t.test('completion to url', async t => {
  const install = new Install({})
  const res = await install.completion({ partialWord: 'http://path/to/url' })
  t.strictSame(res, [])
  t.end()
})

t.test('completion', async t => {
  const install = new Install({})
  const res = await install.completion({ partialWord: 'toto' })
  t.notOk(res)
  t.end()
})
