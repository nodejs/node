const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')
const { resolve, delimiter } = require('path')

const ARB_CTOR = []
const ARB_ACTUAL_TREE = {}
const ARB_REIFY = []
class Arborist {
  constructor (options) {
    ARB_CTOR.push(options)
    this.path = options.path
  }

  async loadActual () {
    return ARB_ACTUAL_TREE[this.path]
  }

  async reify (options) {
    ARB_REIFY.push(options)
  }
}

let PROGRESS_ENABLED = true
const LOG_WARN = []
let PROGRESS_IGNORED = false
const flatOptions = {
  npxCache: 'npx-cache-dir',
  color: false,
  cache: 'cache-dir',
  legacyPeerDeps: false,
  package: [],
}
const config = {
  cache: 'bad-cache-dir', // this should never show up passed into libnpmexec
  yes: true,
  call: '',
  package: [],
  'script-shell': 'shell-cmd',
}

const npm = mockNpm({
  flatOptions,
  config,
  localPrefix: 'local-prefix',
  localBin: 'local-bin',
  globalBin: 'global-bin',
})

const RUN_SCRIPTS = []
const runScript = async opt => {
  RUN_SCRIPTS.push(opt)
  if (!PROGRESS_IGNORED && PROGRESS_ENABLED) {
    throw new Error('progress not disabled during run script!')
  }
}

const MANIFESTS = {}
const pacote = {
  manifest: async (spec, options) => {
    return MANIFESTS[spec]
  },
}

const MKDIRPS = []
const mkdirp = async path => MKDIRPS.push(path)

let READ_RESULT = ''
let READ_ERROR = null
const READ = []
const read = (options, cb) => {
  READ.push(options)
  process.nextTick(() => cb(READ_ERROR, READ_RESULT))
}

let CI_NAME = 'travis-ci'

const log = {
  'proc-log': {
    warn: (...args) => {
      LOG_WARN.push(args)
    },
  },
  npmlog: {
    disableProgress: () => {
      PROGRESS_ENABLED = false
    },
    enableProgress: () => {
      PROGRESS_ENABLED = true
    },
    clearProgress: () => {},
  },
}

const mocks = {
  libnpmexec: t.mock('libnpmexec', {
    '@npmcli/arborist': Arborist,
    '@npmcli/run-script': runScript,
    '@npmcli/ci-detect': () => CI_NAME,
    pacote,
    read,
    'mkdirp-infer-owner': mkdirp,
    ...log,
  }),
  ...log,
}
const Exec = t.mock('../../../lib/commands/exec.js', mocks)
const exec = new Exec(npm)

t.afterEach(() => {
  MKDIRPS.length = 0
  ARB_CTOR.length = 0
  ARB_REIFY.length = 0
  RUN_SCRIPTS.length = 0
  READ.length = 0
  READ_RESULT = ''
  READ_ERROR = null
  LOG_WARN.length = 0
  PROGRESS_IGNORED = false
  flatOptions.legacyPeerDeps = false
  flatOptions.color = false
  config['script-shell'] = 'shell-cmd'
  config.package = []
  flatOptions.package = []
  config.call = ''
  config.yes = true
  npm.color = false
  npm.localBin = 'local-bin'
  npm.globalBin = 'global-bin'
})

t.test('npx foo, bin already exists locally', async t => {
  const path = t.testdir({
    node_modules: {
      '.bin': {
        foo: 'just some file',
      },
    },
  })

  PROGRESS_IGNORED = true
  npm.localBin = resolve(path, 'node_modules', '.bin')

  await exec.exec(['foo', 'one arg', 'two arg'])
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      cache: flatOptions.cache,
      npxCache: flatOptions.npxCache,
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: {
        PATH: [npm.localBin, process.env.PATH].join(delimiter),
      },
      stdio: 'inherit',
    },
  ])
})

t.test('npx foo, bin already exists globally', async t => {
  const path = t.testdir({
    node_modules: {
      '.bin': {
        foo: 'just some file',
      },
    },
  })

  PROGRESS_IGNORED = true
  npm.globalBin = resolve(path, 'node_modules', '.bin')

  await exec.exec(['foo', 'one arg', 'two arg'])
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: {
        PATH: [npm.globalBin, process.env.PATH].join(delimiter),
      },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec foo, already present locally', async t => {
  const path = t.testdir()
  const pkg = { name: 'foo', version: '1.2.3', bin: { foo: 'foo' } }
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec.exec(['foo', 'one arg', 'two arg'])
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec <noargs>, run interactive shell', t => {
  CI_NAME = null
  const { isTTY } = process.stdin
  process.stdin.isTTY = true
  t.teardown(() => (process.stdin.isTTY = isTTY))

  const run = async (t, doRun) => {
    LOG_WARN.length = 0
    ARB_CTOR.length = 0
    MKDIRPS.length = 0
    ARB_REIFY.length = 0
    npm._mockOutputs.length = 0
    await exec.exec([])
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.strictSame(ARB_CTOR, [], 'no need to instantiate arborist')
    t.strictSame(ARB_REIFY, [], 'no need to reify anything')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    if (doRun) {
      t.match(RUN_SCRIPTS, [
        {
          pkg: { scripts: { npx: 'shell-cmd' } },
          args: [],
          banner: false,
          path: process.cwd(),
          stdioString: true,
          event: 'npx',
          env: { PATH: process.env.PATH },
          stdio: 'inherit',
        },
      ])
    } else {
      t.strictSame(RUN_SCRIPTS, [])
    }

    RUN_SCRIPTS.length = 0
  }
  t.test('print message when tty and not in CI', async t => {
    CI_NAME = null
    process.stdin.isTTY = true
    await run(t, true)
    t.strictSame(LOG_WARN, [])
    t.strictSame(
      npm._mockOutputs,
      [
        [
          /* eslint-disable-next-line max-len */
          `\nEntering npm script environment at location:\n${process.cwd()}\nType 'exit' or ^D when finished\n`,
        ],
      ],
      'printed message about interactive shell'
    )
  })

  t.test('print message with color when tty and not in CI', async t => {
    CI_NAME = null
    process.stdin.isTTY = true
    npm.color = true
    flatOptions.color = true

    await run(t, true)
    t.strictSame(LOG_WARN, [])
    t.strictSame(
      npm._mockOutputs,
      [
        [
          /* eslint-disable-next-line max-len */
          `\u001b[0m\u001b[0m\n\u001b[0mEntering npm script environment\u001b[0m\u001b[0m at location:\u001b[0m\n\u001b[0m\u001b[2m${process.cwd()}\u001b[22m\u001b[0m\u001b[1m\u001b[22m\n\u001b[1mType 'exit' or ^D when finished\u001b[22m\n\u001b[1m\u001b[22m`,
        ],
      ],
      'printed message about interactive shell'
    )
  })

  t.test('no message when not TTY', async t => {
    CI_NAME = null
    process.stdin.isTTY = false
    await run(t, true)
    t.strictSame(LOG_WARN, [])
    t.strictSame(npm._mockOutputs, [], 'no message about interactive shell')
  })

  t.test('print warning when in CI and interactive', async t => {
    CI_NAME = 'travis-ci'
    process.stdin.isTTY = true
    await run(t, false)
    t.strictSame(LOG_WARN, [['exec', 'Interactive mode disabled in CI environment']])
    t.strictSame(npm._mockOutputs, [], 'no message about interactive shell')
  })

  t.test('not defined script-shell config value', async t => {
    CI_NAME = null
    process.stdin.isTTY = true
    config['script-shell'] = undefined

    await exec.exec([])

    t.match(RUN_SCRIPTS, [
      {
        pkg: { scripts: { npx: /sh|cmd/ } },
      },
    ])

    LOG_WARN.length = 0
    ARB_CTOR.length = 0
    MKDIRPS.length = 0
    ARB_REIFY.length = 0
    npm._mockOutputs.length = 0
    RUN_SCRIPTS.length = 0
  })

  t.end()
})

t.test('npm exec foo, not present locally or in central loc', async t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec.exec(['foo', 'one arg', 'two arg'])
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.match(ARB_REIFY, [{ add: ['foo@'], legacyPeerDeps: false }], 'need to install foo@')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec foo, packageLockOnly set to true', async t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
  npm.localPrefix = path
  npm.config.set('package-lock-only', true)
  t.teardown(() => {
    npm.config.set('package-lock-only', false)
  })

  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec.exec(['foo', 'one arg', 'two arg'])
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{
    path,
    packageLockOnly: false,
  }])
  t.match(ARB_REIFY, [{
    add: ['foo@'],
    legacyPeerDeps: false,
    packageLockOnly: false,
  }], 'need to install foo@')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec foo, not present locally but in central loc', async t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
  const pkg = { name: 'foo', version: '1.2.3', bin: { foo: 'foo' } }
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec.exec(['foo', 'one arg', 'two arg'])
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no need to install again, already there')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec foo, present locally but wrong version', async t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/2badf4630f1cfaad')
  const pkg = { name: 'foo', version: '1.2.3', bin: { foo: 'foo' } }
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }
  MANIFESTS['foo@2.x'] = {
    name: 'foo',
    version: '2.3.4',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@2.x',
  }
  await exec.exec(['foo@2.x', 'one arg', 'two arg'])
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.match(ARB_REIFY, [{ add: ['foo@2.x'], legacyPeerDeps: false }], 'need to add foo@2.x')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec foo, present locally but outdated version', async t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
  const pkg = { name: 'foo', version: '1.2.3', bin: { foo: 'foo' } }
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '2.3.4',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@2.x',
  }
  await exec.exec(['foo', 'one arg', 'two arg'])
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.match(ARB_REIFY, [{ add: ['foo'], legacyPeerDeps: false }], 'need to add foo@2.x')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec --package=foo bar', async t => {
  const path = t.testdir()
  const pkg = { name: 'foo', version: '1.2.3', bin: { foo: 'foo' } }
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  config.package = ['foo']
  flatOptions.package = ['foo']
  await exec.exec(['bar', 'one arg', 'two arg'])
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'bar' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('npm exec @foo/bar -- --some=arg, locally installed', async t => {
  const foobarManifest = {
    name: '@foo/bar',
    version: '1.2.3',
    bin: {
      foo: 'foo',
      bar: 'bar',
    },
  }
  const path = t.testdir({
    node_modules: {
      '@foo/bar': {
        'package.json': JSON.stringify(foobarManifest),
      },
    },
  })
  const pkg = {
    name: '@foo/bar',
    version: '1.2.3',
    bin: { foo: 'foo', bar: 'bar' },
  }
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }
  MANIFESTS['@foo/bar'] = foobarManifest
  await exec.exec(['@foo/bar', '--some=arg'])
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'bar' } },
      args: ['--some=arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    },
  ])
})

t.test(
  'npm exec @foo/bar, with same bin alias and no unscoped named bin, locally installed',
  async t => {
    const pkg = {
      name: '@foo/bar',
      version: '1.2.3',
      bin: {
        baz: 'corge', // pick the first one
        qux: 'corge',
        quux: 'corge',
      },
    }
    const path = t.testdir({
      node_modules: {
        '@foo/bar': {
          'package.json': JSON.stringify(pkg),
        },
      },
    })
    npm.localPrefix = path
    ARB_ACTUAL_TREE[path] = {
      inventory: {
        query () {
          return new Set([{ ...pkg, package: pkg }])
        },
      },
    }
    MANIFESTS['@foo/bar'] = pkg
    await exec.exec(['@foo/bar', 'one arg', 'two arg'])
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no need to reify anything')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.match(RUN_SCRIPTS, [
      {
        pkg: { scripts: { npx: 'baz' } },
        args: ['one arg', 'two arg'],
        banner: false,
        path: process.cwd(),
        stdioString: true,
        event: 'npx',
        env: { PATH: process.env.PATH },
        stdio: 'inherit',
      },
    ])
  }
)

t.test(
  'npm exec @foo/bar, with different bin alias and no unscoped named bin, locally installed',
  async t => {
    const path = t.testdir()
    const pkg = {
      name: '@foo/bar',
      version: '1.2.3.',
      bin: { foo: 'qux', corge: 'qux', baz: 'quux' },
    }
    npm.localPrefix = path
    ARB_ACTUAL_TREE[path] = {
      inventory: {
        query () {
          return new Set([{
            ...pkg,
            package: pkg,
            pkgid: `${pkg.name}@${pkg.version}`,
          }])
        },
      },
    }
    MANIFESTS['@foo/bar'] = {
      name: '@foo/bar',
      version: '1.2.3',
      bin: {
        foo: 'qux',
        corge: 'qux',
        baz: 'quux',
      },
      _from: 'foo@',
      _id: '@foo/bar@1.2.3',
    }
    await t.rejects(exec.exec(['@foo/bar']), {
      message: 'could not determine executable to run',
      pkgid: '@foo/bar@1.2.3',
    })
  }
)

t.test('run command with 2 packages, need install, verify sort', async t => {
  // test both directions, should use same install dir both times
  // also test the read() call here, verify that the prompts match
  const cases = [
    ['foo', 'bar'],
    ['bar', 'foo'],
  ]
  t.plan(cases.length)
  for (const packages of cases) {
    t.test(packages.join(', '), async t => {
      config.package = packages
      const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b, 'en'))
      const path = t.testdir()
      const installDir = resolve('npx-cache-dir/07de77790e5f40f2')
      npm.localPrefix = path
      ARB_ACTUAL_TREE[path] = {
        inventory: {
          query () {
            return new Set()
          },
        },
      }
      ARB_ACTUAL_TREE[installDir] = {
        inventory: {
          query () {
            return new Set()
          },
        },
      }
      MANIFESTS.foo = {
        name: 'foo',
        version: '1.2.3',
        bin: {
          foo: 'foo',
        },
        _from: 'foo@',
      }
      MANIFESTS.bar = {
        name: 'bar',
        version: '1.2.3',
        bin: {
          bar: 'bar',
        },
        _from: 'bar@',
      }
      await exec.exec(['foobar', 'one arg', 'two arg'])
      t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
      t.match(ARB_CTOR, [{ path }])
      t.match(ARB_REIFY, [{ add, legacyPeerDeps: false }], 'need to install both packages')
      t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
      const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
      t.match(RUN_SCRIPTS, [
        {
          pkg: { scripts: { npx: 'foobar' } },
          args: ['one arg', 'two arg'],
          banner: false,
          path: process.cwd(),
          stdioString: true,
          event: 'npx',
          env: { PATH },
          stdio: 'inherit',
        },
      ])
    })
  }
})

t.test('npm exec foo, no bin in package', async t => {
  const pkg = { name: 'foo', version: '1.2.3' }
  const path = t.testdir({
    node_modules: {
      foo: {
        'package.json': JSON.stringify(pkg),
      },
    },
  })
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set([{
          ...pkg,
          package: pkg,
          pkgid: `${pkg.name}@${pkg.version}`,
        }])
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    _from: 'foo@',
    _id: 'foo@1.2.3',
  }
  await t.rejects(exec.exec(['foo']), {
    message: 'could not determine executable to run',
    pkgid: 'foo@1.2.3',
  })
})

t.test('npm exec foo, many bins in package, none named foo', async t => {
  const path = t.testdir()
  const pkg = {
    name: 'foo',
    version: '1.2.3',
    bin: { bar: 'bar', baz: 'baz' },
  }
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set([{
          ...pkg,
          package: pkg,
          pkgid: `${pkg.name}@${pkg.version}`,
        }])
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      bar: 'bar',
      baz: 'baz',
    },
    _from: 'foo@',
    _id: 'foo@1.2.3',
  }
  await t.rejects(exec.exec(['foo']), {
    message: 'could not determine executable to run',
    pkgid: 'foo@1.2.3',
  })
})

t.test('npm exec -p foo -c "ls -laF"', async t => {
  const path = t.testdir()
  const pkg = { name: 'foo', version: '1.2.3' }
  npm.localPrefix = path
  config.package = ['foo']
  config.call = 'ls -laF'
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    _from: 'foo@',
  }
  await exec.exec([])
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'ls -laF' } },
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    },
  ])
})

t.test('positional args and --call together is an error', async t => {
  config.call = 'true'
  await t.rejects(exec.exec(['foo']), exec.usage)
})

t.test('prompt when installs are needed if not already present and shell is a TTY', async t => {
  const stdoutTTY = process.stdout.isTTY
  const stdinTTY = process.stdin.isTTY
  t.teardown(() => {
    process.stdout.isTTY = stdoutTTY
    process.stdin.isTTY = stdinTTY
    CI_NAME = 'travis-ci'
  })
  process.stdout.isTTY = true
  process.stdin.isTTY = true
  CI_NAME = false

  const packages = ['foo', 'bar']
  READ_RESULT = 'yolo'

  config.package = packages
  config.yes = undefined

  const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b, 'en'))
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  MANIFESTS.bar = {
    name: 'bar',
    version: '1.2.3',
    bin: {
      bar: 'bar',
    },
    _from: 'bar@',
  }
  await exec.exec(['foobar'])
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.match(ARB_REIFY, [{ add, legacyPeerDeps: false }], 'need to install both packages')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foobar' } },
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    },
  ])
  t.strictSame(READ, [
    {
      prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
      default: 'y',
    },
  ])
})

t.test(
  /* eslint-disable-next-line max-len */
  'skip prompt when installs are needed if not already present and shell is not a tty (multiple packages)',
  async t => {
    const stdoutTTY = process.stdout.isTTY
    const stdinTTY = process.stdin.isTTY
    t.teardown(() => {
      process.stdout.isTTY = stdoutTTY
      process.stdin.isTTY = stdinTTY
      CI_NAME = 'travis-ci'
    })
    process.stdout.isTTY = false
    process.stdin.isTTY = false
    CI_NAME = false

    const packages = ['foo', 'bar']
    READ_RESULT = 'yolo'

    config.package = packages
    config.yes = undefined

    const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b, 'en'))
    const path = t.testdir()
    const installDir = resolve('npx-cache-dir/07de77790e5f40f2')
    npm.localPrefix = path
    ARB_ACTUAL_TREE[path] = {
      inventory: {
        query () {
          return new Set()
        },
      },
    }
    ARB_ACTUAL_TREE[installDir] = {
      inventory: {
        query () {
          return new Set()
        },
      },
    }
    MANIFESTS.foo = {
      name: 'foo',
      version: '1.2.3',
      bin: {
        foo: 'foo',
      },
      _from: 'foo@',
    }
    MANIFESTS.bar = {
      name: 'bar',
      version: '1.2.3',
      bin: {
        bar: 'bar',
      },
      _from: 'bar@',
    }
    await exec.exec(['foobar'])
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.match(ARB_REIFY, [{ add, legacyPeerDeps: false }], 'need to install both packages')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
    t.match(RUN_SCRIPTS, [
      {
        pkg: { scripts: { npx: 'foobar' } },
        banner: false,
        path: process.cwd(),
        stdioString: true,
        event: 'npx',
        env: { PATH },
        stdio: 'inherit',
      },
    ])
    t.strictSame(READ, [], 'should not have prompted')
    t.strictSame(
      LOG_WARN,
      [['exec', 'The following packages were not found and will be installed: bar, foo']],
      'should have printed a warning'
    )
  }
)

t.test(
  /* eslint-disable-next-line max-len */
  'skip prompt when installs are needed if not already present and shell is not a tty (single package)',
  async t => {
    const stdoutTTY = process.stdout.isTTY
    const stdinTTY = process.stdin.isTTY
    t.teardown(() => {
      process.stdout.isTTY = stdoutTTY
      process.stdin.isTTY = stdinTTY
      CI_NAME = 'travis-ci'
    })
    process.stdout.isTTY = false
    process.stdin.isTTY = false
    CI_NAME = false

    const packages = ['foo']
    READ_RESULT = 'yolo'

    config.package = packages
    config.yes = undefined

    const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b, 'en'))
    const path = t.testdir()
    const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
    npm.localPrefix = path
    ARB_ACTUAL_TREE[path] = {
      inventory: {
        query () {
          return new Set()
        },
      },
    }
    ARB_ACTUAL_TREE[installDir] = {
      inventory: {
        query () {
          return new Set()
        },
      },
    }
    MANIFESTS.foo = {
      name: 'foo',
      version: '1.2.3',
      bin: {
        foo: 'foo',
      },
      _from: 'foo@',
    }
    await exec.exec(['foobar'])
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.match(ARB_REIFY, [{ add, legacyPeerDeps: false }], 'need to install the package')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
    t.match(RUN_SCRIPTS, [
      {
        pkg: { scripts: { npx: 'foobar' } },
        banner: false,
        path: process.cwd(),
        stdioString: true,
        event: 'npx',
        env: { PATH },
        stdio: 'inherit',
      },
    ])
    t.strictSame(READ, [], 'should not have prompted')
    t.strictSame(
      LOG_WARN,
      [['exec', 'The following package was not found and will be installed: foo']],
      'should have printed a warning'
    )
  }
)

t.test('abort if prompt rejected', async t => {
  const stdoutTTY = process.stdout.isTTY
  const stdinTTY = process.stdin.isTTY
  t.teardown(() => {
    process.stdout.isTTY = stdoutTTY
    process.stdin.isTTY = stdinTTY
    CI_NAME = 'travis-ci'
  })
  process.stdout.isTTY = true
  process.stdin.isTTY = true
  CI_NAME = false

  const packages = ['foo', 'bar']
  READ_RESULT = 'no, why would I want such a thing??'

  config.package = packages
  config.yes = undefined

  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  MANIFESTS.bar = {
    name: 'bar',
    version: '1.2.3',
    bin: {
      bar: 'bar',
    },
    _from: 'bar@',
  }
  await t.rejects(exec.exec(['foobar']), /canceled/, 'should be canceled')
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no install performed')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.strictSame(RUN_SCRIPTS, [])
  t.strictSame(READ, [
    {
      prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
      default: 'y',
    },
  ])
})

t.test('abort if prompt false', async t => {
  const stdoutTTY = process.stdout.isTTY
  const stdinTTY = process.stdin.isTTY
  t.teardown(() => {
    process.stdout.isTTY = stdoutTTY
    process.stdin.isTTY = stdinTTY
    CI_NAME = 'travis-ci'
  })
  process.stdout.isTTY = true
  process.stdin.isTTY = true
  CI_NAME = false

  const packages = ['foo', 'bar']
  READ_ERROR = 'canceled'

  config.package = packages
  config.yes = undefined

  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  MANIFESTS.bar = {
    name: 'bar',
    version: '1.2.3',
    bin: {
      bar: 'bar',
    },
    _from: 'bar@',
  }
  await t.rejects(exec.exec(['foobar']), 'canceled', 'should be canceled')
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no install performed')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.strictSame(RUN_SCRIPTS, [])
  t.strictSame(READ, [
    {
      prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
      default: 'y',
    },
  ])
})

t.test('abort if -n provided', async t => {
  const stdoutTTY = process.stdout.isTTY
  const stdinTTY = process.stdin.isTTY
  t.teardown(() => {
    process.stdout.isTTY = stdoutTTY
    process.stdin.isTTY = stdinTTY
    CI_NAME = 'travis-ci'
  })
  process.stdout.isTTY = true
  process.stdin.isTTY = true
  CI_NAME = false

  const packages = ['foo', 'bar']

  config.package = packages
  config.yes = false

  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  MANIFESTS.bar = {
    name: 'bar',
    version: '1.2.3',
    bin: {
      bar: 'bar',
    },
    _from: 'bar@',
  }
  await t.rejects(exec.exec(['foobar']), /canceled/, 'should be canceled')
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ path }])
  t.strictSame(ARB_REIFY, [], 'no install performed')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.strictSame(RUN_SCRIPTS, [])
  t.strictSame(READ, [])
})

t.test('forward legacyPeerDeps opt', async t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  ARB_ACTUAL_TREE[installDir] = {
    inventory: {
      query () {
        return new Set()
      },
    },
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  config.yes = true
  flatOptions.legacyPeerDeps = true
  await exec.exec(['foo'])
  t.match(
    ARB_REIFY,
    [{ add: ['foo@'], legacyPeerDeps: true }],
    'need to install foo@ using legacyPeerDeps opt'
  )
})

t.test('workspaces', async t => {
  npm.localPrefix = t.testdir({
    node_modules: {
      '.bin': {
        a: '',
        foo: '',
      },
    },
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          bin: 'cli.js',
        }),
        'cli.js': '',
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '1.0.0',
        }),
      },
    },
    'package.json': JSON.stringify({
      name: 'root',
      version: '1.0.0',
      workspaces: ['packages/*'],
    }),
  })

  const pkg = { name: 'foo', version: '1.2.3', bin: { foo: 'foo' } }
  PROGRESS_IGNORED = true
  npm.localBin = resolve(npm.localPrefix, 'node_modules', '.bin')

  // with arg matching existing bin, run scripts in the context of a workspace
  await exec.execWorkspaces(['foo', 'one arg', 'two arg'], ['a', 'b'])

  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: npm.localPrefix,
      stdioString: true,
      event: 'npx',
      env: {
        PATH: [npm.localBin, process.env.PATH].join(delimiter),
      },
      stdio: 'inherit',
    },
    {
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: npm.localPrefix,
      stdioString: true,
      event: 'npx',
      env: {
        PATH: [npm.localBin, process.env.PATH].join(delimiter),
      },
      stdio: 'inherit',
    },
  ], 'should run with multiple args across multiple workspaces')

  // clean up
  RUN_SCRIPTS.length = 0

  // with packages, run scripts in the context of a workspace
  config.package = ['foo']
  config.call = 'foo'
  config.yes = false

  ARB_ACTUAL_TREE[npm.localPrefix] = {
    inventory: {
      query () {
        return new Set([{ ...pkg, package: pkg }])
      },
    },
  }

  await exec.execWorkspaces([], ['a', 'b'])

  // path should point to the workspace folder
  t.match(RUN_SCRIPTS, [
    {
      pkg: { scripts: { npx: 'foo' } },
      args: [],
      banner: false,
      path: resolve(npm.localPrefix, 'packages', 'a'),
      stdioString: true,
      event: 'npx',
      stdio: 'inherit',
    },
    {
      pkg: { scripts: { npx: 'foo' } },
      args: [],
      banner: false,
      path: resolve(npm.localPrefix, 'packages', 'b'),
      stdioString: true,
      event: 'npx',
      stdio: 'inherit',
    },
  ], 'should run without args in multiple workspaces')

  t.match(ARB_CTOR, [
    { path: npm.localPrefix },
    { path: npm.localPrefix },
  ])

  // no args, spawn interactive shell
  CI_NAME = null
  config.package = []
  config.call = ''
  process.stdin.isTTY = true

  await exec.execWorkspaces([], ['a'])

  t.strictSame(LOG_WARN, [])
  t.strictSame(
    npm._mockOutputs,
    [
      [
        `\nEntering npm script environment in workspace a@1.0.0 at location:\n${resolve(
          npm.localPrefix,
          'packages/a'
        )}\nType 'exit' or ^D when finished\n`,
      ],
    ],
    'printed message about interactive shell'
  )

  npm.color = true
  flatOptions.color = true
  npm._mockOutputs.length = 0
  await exec.execWorkspaces([], ['a'])

  t.strictSame(LOG_WARN, [])
  t.strictSame(
    npm._mockOutputs,
    [
      [
        /* eslint-disable-next-line max-len */
        `\u001b[0m\u001b[0m\n\u001b[0mEntering npm script environment\u001b[0m\u001b[0m in workspace \u001b[32ma@1.0.0\u001b[39m at location:\u001b[0m\n\u001b[0m\u001b[2m${resolve(
          npm.localPrefix,
          'packages/a'
          /* eslint-disable-next-line max-len */
        )}\u001b[22m\u001b[0m\u001b[1m\u001b[22m\n\u001b[1mType 'exit' or ^D when finished\u001b[22m\n\u001b[1m\u001b[22m`,
      ],
    ],
    'printed message about interactive shell'
  )
})
