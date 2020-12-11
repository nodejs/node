const t = require('tap')

process.env.NODE = '/path/to/some/node'
process.env.NODE_ENV = 'development'

const logs = []
const log = require('npmlog')
log.warn = (...args) => logs.push(['warn', ...args])
log.verbose = (...args) => logs.push(['verbose', ...args])

class Mocknpm {
  constructor (opts = {}) {
    this.modes = {
      exec: 0o777,
      file: 0o666,
      umask: 0o22,
    }
    this.color = true
    this.projectScope = '@npmcli'
    this.tmp = '/tmp'
    this.command = null
    this.globalPrefix = '/usr/local'
    this.localPrefix = '/path/to/npm/cli'
    this.prefix = this.localPrefix
    this.version = '7.6.5'
    this.config = new MockConfig(opts)
    this.flatOptions = null
  }
}

class MockConfig {
  constructor (opts = {}) {
    this.list = [{
      cache: 'cache',
      'node-version': '1.2.3',
      global: 'global',
      'metrics-registry': 'metrics-registry',
      'send-metrics': 'send-metrics',
      registry: 'registry',
      access: 'access',
      'always-auth': 'always-auth',
      audit: 'audit',
      'audit-level': 'audit-level',
      'auth-type': 'auth-type',
      before: 'before',
      browser: 'browser',
      ca: 'ca',
      cafile: 'cafile',
      call: 'call',
      cert: 'cert',
      key: 'key',
      'cache-lock-retries': 'cache-lock-retries',
      'cache-lock-stale': 'cache-lock-stale',
      'cache-lock-wait': 'cache-lock-wait',
      cidr: 'cidr',
      'read-only': 'read-only',
      preid: 'preid',
      'tag-version-prefix': 'tag-version-prefix',
      'allow-same-version': 'allow-same-version',
      message: 'message',
      'commit-hooks': 'commit-hooks',
      'git-tag-version': 'git-tag-version',
      'sign-git-commit': 'sign-git-commit',
      'sign-git-tag': 'sign-git-tag',
      depth: 'depth',
      description: 'description',
      searchexclude: 'searchexclude',
      searchlimit: 'searchlimit',
      searchopts: 'from=1',
      searchstaleness: 'searchstaleness',
      'dry-run': 'dry-run',
      'engine-strict': 'engine-strict',
      'fetch-retries': 'fetch-retries',
      'fetch-retry-factor': 'fetch-retry-factor',
      'fetch-retry-mintimeout': 'fetch-retry-mintimeout',
      'fetch-retry-maxtimeout': 'fetch-retry-maxtimeout',
      'fetch-timeout': 'fetch-timeout',
      force: 'force',
      'format-package-lock': 'format-package-lock',
      fund: 'fund',
      git: 'git',
      viewer: 'viewer',
      editor: 'editor',
      'bin-links': 'bin-links',
      'rebuild-bundle': 'rebuild-bundle',
      package: 'package',
      'package-lock': 'package-lock',
      'package-lock-only': 'package-lock-only',
      'global-style': 'global-style',
      'legacy-bundling': 'legacy-bundling',
      'script-shell': 'script-shell',
      omit: [],
      include: [],
      save: 'save',
      'save-bundle': 'save-bundle',
      'save-dev': 'save-dev',
      'save-optional': 'save-optional',
      'save-peer': 'save-peer',
      'save-prod': 'save-prod',
      'save-exact': 'save-exact',
      'save-prefix': 'save-prefix',
      otp: 'otp',
      offline: 'offline',
      'prefer-online': 'prefer-online',
      'prefer-offline': 'prefer-offline',
      'cache-max': 'cache-max',
      'cache-min': 'cache-min',
      'strict-ssl': 'strict-ssl',
      scope: '',
      tag: 'tag',
      'user-agent': 'user-agent',
      '@scope:registry': '@scope:registry',
      '//nerf.dart:_authToken': '//nerf.dart:_authToken',
      proxy: 'proxy',
      noproxy: 'noproxy',
      ...opts,
    }]
  }

  get (key) {
    return this.list[0][key]
  }

  set (key, val) {
    this.list[0][key] = val
  }
}

const flatOptions = require('../../../lib/utils/flat-options.js')
t.match(logs, [[
  'verbose',
  'npm-session',
  /^[0-9a-f]{16}$/,
]], 'logged npm session verbosely')
logs.length = 0

t.test('basic', t => {
  const npm = new Mocknpm()
  const generatedFlat = flatOptions(npm)
  const clean = {
    ...generatedFlat,
    npmBin: '/path/to/npm/bin.js',
    log: {},
    npmSession: '12345',
    cache: generatedFlat.cache.replace(/\\/g, '/'),
  }
  t.matchSnapshot(clean, 'flat options')
  t.equal(generatedFlat.npmCommand, null, 'command not set yet')
  npm.command = 'view'
  t.equal(generatedFlat.npmCommand, 'view', 'command updated via getter')
  t.equal(generatedFlat.npmBin, require.main.filename)
  // test the object is frozen
  generatedFlat.newField = 'asdf'
  t.equal(generatedFlat.newField, undefined, 'object is frozen')
  const preExistingOpts = { flat: 'options' }
  npm.flatOptions = preExistingOpts
  t.equal(flatOptions(npm), preExistingOpts, 'use pre-existing npm.flatOptions')
  t.end()
})

t.test('get preferOffline from cache-min', t => {
  const npm = new Mocknpm({
    'cache-min': 9999999,
    'prefer-offline': undefined,
  })
  const opts = flatOptions(npm)
  t.equal(opts.preferOffline, true, 'got preferOffline from cache min')
  logs.length = 0
  t.equal(opts.cacheMin, undefined, 'opts.cacheMin is not set')
  t.match(logs, [])
  logs.length = 0
  t.end()
})

t.test('get preferOnline from cache-max', t => {
  const npm = new Mocknpm({
    'cache-max': -1,
    'prefer-online': undefined,
  })
  const opts = flatOptions(npm)
  t.equal(opts.preferOnline, true, 'got preferOnline from cache min')
  logs.length = 0
  t.equal(opts.cacheMax, undefined, 'opts.cacheMax is not set')
  t.match(logs, [])
  logs.length = 0
  t.end()
})

t.test('tag emits warning', t => {
  const npm = new Mocknpm({ tag: 'foobar' })
  t.equal(flatOptions(npm).tag, 'foobar', 'tag is foobar')
  t.match(logs, [])
  logs.length = 0
  t.end()
})

t.test('omit/include options', t => {
  t.test('omit explicitly', t => {
    const { NODE_ENV } = process.env
    const npm = new Mocknpm({
      omit: ['dev', 'optional', 'peer'],
    })
    t.strictSame(flatOptions(npm).omit, ['dev', 'optional', 'peer'])
    t.equal(process.env.NODE_ENV, 'production')
    process.env.NODE_ENV = NODE_ENV
    t.end()
  })

  t.test('omit and include some', t => {
    const { NODE_ENV } = process.env
    const npm = new Mocknpm({
      omit: ['dev', 'optional', 'peer'],
      include: ['peer'],
    })
    t.strictSame(flatOptions(npm).omit, ['dev', 'optional'])
    t.equal(process.env.NODE_ENV, 'production')
    process.env.NODE_ENV = NODE_ENV
    t.end()
  })

  t.test('dev flag', t => {
    const { NODE_ENV } = process.env
    const npm = new Mocknpm({
      omit: ['dev', 'optional', 'peer'],
      include: [],
      dev: true,
    })
    t.strictSame(flatOptions(npm).omit, ['optional', 'peer'])
    t.equal(process.env.NODE_ENV, NODE_ENV)
    process.env.NODE_ENV = NODE_ENV
    t.end()
  })

  t.test('production flag', t => {
    const { NODE_ENV } = process.env
    const npm = new Mocknpm({
      omit: [],
      include: [],
      production: true,
    })
    t.strictSame(flatOptions(npm).omit, ['dev'])
    t.equal(process.env.NODE_ENV, 'production')
    process.env.NODE_ENV = NODE_ENV
    t.end()
  })

  t.test('only', t => {
    const { NODE_ENV } = process.env
    const cases = ['prod', 'production']
    t.plan(cases.length)
    cases.forEach(c => t.test(c, t => {
      const npm = new Mocknpm({
        omit: [],
        include: [],
        only: c,
      })
      t.strictSame(flatOptions(npm).omit, ['dev'])
      t.equal(process.env.NODE_ENV, 'production')
      process.env.NODE_ENV = NODE_ENV
      t.end()
    }))
  })

  t.test('also dev', t => {
    const { NODE_ENV } = process.env
    const npm = new Mocknpm({
      omit: ['dev', 'optional', 'peer'],
      also: 'dev',
    })
    t.strictSame(flatOptions(npm).omit, ['optional', 'peer'])
    t.equal(process.env.NODE_ENV, NODE_ENV)
    process.env.NODE_ENV = NODE_ENV
    t.end()
  })

  t.test('no-optional', t => {
    const { NODE_ENV } = process.env
    const npm = new Mocknpm({
      optional: false,
      omit: null,
      include: null,
    })
    t.strictSame(flatOptions(npm).omit, ['optional'])
    t.equal(process.env.NODE_ENV, NODE_ENV)
    process.env.NODE_ENV = NODE_ENV
    t.end()
  })

  t.end()
})

t.test('get the node without the environ', t => {
  delete process.env.NODE
  t.equal(flatOptions(new Mocknpm()).nodeBin, process.execPath)
  t.end()
})

t.test('various default values and falsey fallbacks', t => {
  const npm = new Mocknpm({
    'script-shell': false,
    registry: 'http://example.com',
    'metrics-registry': null,
    searchlimit: 0,
    'save-exact': false,
    'save-prefix': '>=',
  })
  const opts = flatOptions(npm)
  t.equal(opts.scriptShell, undefined, 'scriptShell is undefined if falsey')
  t.equal(opts.metricsRegistry, 'http://example.com',
    'metricsRegistry defaults to registry')
  t.equal(opts.search.limit, 20, 'searchLimit defaults to 20')
  t.equal(opts.savePrefix, '>=', 'save-prefix respected if no save-exact')
  t.equal(opts.scope, '', 'scope defaults to empty string')
  logs.length = 0
  t.end()
})

t.test('legacy _auth token', t => {
  const npm = new Mocknpm({
    _auth: 'asdfasdf',
  })
  t.strictSame(
    flatOptions(npm)._auth,
    'asdfasdf',
    'should set legacy _auth token'
  )
  t.end()
})

t.test('save-type', t => {
  const base = {
    'save-optional': false,
    'save-peer': false,
    'save-dev': false,
    'save-prod': false,
  }
  const cases = [
    ['peerOptional', {
      'save-optional': true,
      'save-peer': true,
    }],
    ['optional', {
      'save-optional': true,
    }],
    ['dev', {
      'save-dev': true,
    }],
    ['peer', {
      'save-peer': true,
    }],
    ['prod', {
      'save-prod': true,
    }],
    [null, {}],
  ]
  for (const [expect, options] of cases) {
    const opts = flatOptions(new Mocknpm({
      ...base,
      ...options,
    }))
    t.equal(opts.saveType, expect, JSON.stringify(options))
  }
  t.end()
})
