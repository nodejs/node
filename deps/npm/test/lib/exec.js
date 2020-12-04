const t = require('tap')
const requireInject = require('require-inject')
const { resolve, delimiter } = require('path')
const OUTPUT = []
const output = (...msg) => OUTPUT.push(msg)

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
const npm = {
  flatOptions: {
    yes: true,
    call: '',
    package: [],
    legacyPeerDeps: false,
    shell: 'shell-cmd',
  },
  localPrefix: 'local-prefix',
  localBin: 'local-bin',
  globalBin: 'global-bin',
  config: {
    get: k => {
      if (k !== 'cache')
        throw new Error('unexpected config get')

      return 'cache-dir'
    },
  },
  log: {
    disableProgress: () => {
      PROGRESS_ENABLED = false
    },
    enableProgress: () => {
      PROGRESS_ENABLED = true
    },
    warn: (...args) => {
      LOG_WARN.push(args)
    },
  },
}

const RUN_SCRIPTS = []
const runScript = async opt => {
  RUN_SCRIPTS.push(opt)
  if (!PROGRESS_IGNORED && PROGRESS_ENABLED)
    throw new Error('progress not disabled during run script!')
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

const PATH = require('../../lib/utils/path.js')

let CI_NAME = 'travis-ci'

const mocks = {
  '@npmcli/arborist': Arborist,
  '@npmcli/run-script': runScript,
  '@npmcli/ci-detect': () => CI_NAME,
  '../../lib/npm.js': npm,
  pacote,
  read,
  'mkdirp-infer-owner': mkdirp,
  '../../lib/utils/output.js': output,
}
const exec = requireInject('../../lib/exec.js', mocks)

t.afterEach(cb => {
  MKDIRPS.length = 0
  ARB_CTOR.length = 0
  ARB_REIFY.length = 0
  RUN_SCRIPTS.length = 0
  READ.length = 0
  READ_RESULT = ''
  READ_ERROR = null
  LOG_WARN.length = 0
  PROGRESS_IGNORED = false
  npm.flatOptions.legacyPeerDeps = false
  npm.flatOptions.package = []
  npm.flatOptions.call = ''
  npm.localBin = 'local-bin'
  npm.globalBin = 'global-bin'
  cb()
})

t.test('npx foo, bin already exists locally', async t => {
  const path = t.testdir({
    foo: 'just some file',
  })

  PROGRESS_IGNORED = true
  npm.localBin = path

  await exec(['foo'], er => {
    t.ifError(er, 'npm exec')
  })
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foo' }},
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: {
      PATH: [path, ...PATH].join(delimiter),
    },
    stdio: 'inherit',
  }])
})

t.test('npx foo, bin already exists globally', async t => {
  const path = t.testdir({
    foo: 'just some file',
  })

  PROGRESS_IGNORED = true
  npm.globalBin = path

  await exec(['foo'], er => {
    t.ifError(er, 'npm exec')
  })
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foo' }},
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: {
      PATH: [path, ...PATH].join(delimiter),
    },
    stdio: 'inherit',
  }])
})

t.test('npm exec foo, already present locally', async t => {
  const path = t.testdir()
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec(['foo'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ package: ['foo'], path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foo' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH: process.env.PATH },
    stdio: 'inherit',
  }])
})

t.test('npm exec <noargs>, run interactive shell', async t => {
  CI_NAME = null
  const { isTTY } = process.stdin
  process.stdin.isTTY = true
  t.teardown(() => process.stdin.isTTY = isTTY)

  const run = async (t, doRun = true) => {
    LOG_WARN.length = 0
    ARB_CTOR.length = 0
    MKDIRPS.length = 0
    ARB_REIFY.length = 0
    OUTPUT.length = 0
    await exec([], er => {
      if (er)
        throw er
    })
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.strictSame(ARB_CTOR, [], 'no need to instantiate arborist')
    t.strictSame(ARB_REIFY, [], 'no need to reify anything')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    if (doRun) {
      t.match(RUN_SCRIPTS, [{
        pkg: { scripts: { npx: 'shell-cmd' } },
        banner: false,
        path: process.cwd(),
        stdioString: true,
        event: 'npx',
        env: { PATH: process.env.PATH },
        stdio: 'inherit',
      }])
    } else
      t.strictSame(RUN_SCRIPTS, [])
    RUN_SCRIPTS.length = 0
  }

  t.test('print message when tty and not in CI', async t => {
    CI_NAME = null
    process.stdin.isTTY = true
    await run(t)
    t.strictSame(LOG_WARN, [])
    t.strictSame(OUTPUT, [
      ['\nEntering npm script environment\nType \'exit\' or ^D when finished\n'],
    ], 'printed message about interactive shell')
  })

  t.test('no message when not TTY', async t => {
    CI_NAME = null
    process.stdin.isTTY = false
    await run(t)
    t.strictSame(LOG_WARN, [])
    t.strictSame(OUTPUT, [], 'no message about interactive shell')
  })

  t.test('print warning when in CI and interactive', async t => {
    CI_NAME = 'travis-ci'
    process.stdin.isTTY = true
    await run(t, false)
    t.strictSame(LOG_WARN, [
      ['exec', 'Interactive mode disabled in CI environment'],
    ])
    t.strictSame(OUTPUT, [], 'no message about interactive shell')
  })

  t.end()
})

t.test('npm exec foo, not present locally or in central loc', async t => {
  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/f7fbba6e0636f890')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec(['foo'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: ['foo'], path }])
  t.match(ARB_REIFY, [{add: ['foo@'], legacyPeerDeps: false}], 'need to install foo@')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foo' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH },
    stdio: 'inherit',
  }])
})

t.test('npm exec foo, not present locally but in central loc', async t => {
  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/f7fbba6e0636f890')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec(['foo'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: ['foo'], path }])
  t.match(ARB_REIFY, [], 'no need to install again, already there')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foo' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH },
    stdio: 'inherit',
  }])
})

t.test('npm exec foo, present locally but wrong version', async t => {
  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/2badf4630f1cfaad')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
  }
  MANIFESTS['foo@2.x'] = {
    name: 'foo',
    version: '2.3.4',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@2.x',
  }
  await exec(['foo@2.x'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: ['foo'], path }])
  t.match(ARB_REIFY, [{ add: ['foo@2.x'], legacyPeerDeps: false }], 'need to add foo@2.x')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foo' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH },
    stdio: 'inherit',
  }])
})

t.test('npm exec --package=foo bar', async t => {
  const path = t.testdir()
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  npm.flatOptions.package = ['foo']
  await exec(['bar'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ package: ['foo'], path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'bar' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH: process.env.PATH },
    stdio: 'inherit',
  }])
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
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['@foo/bar', { name: '@foo/bar', version: '1.2.3' }]]),
  }
  MANIFESTS['@foo/bar'] = foobarManifest
  await exec(['@foo/bar', '--some=arg'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ package: ['@foo/bar'], path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'bar' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH: process.env.PATH },
    stdio: 'inherit',
  }])
})

t.test('npm exec @foo/bar, with same bin alias and no unscoped named bin, locally installed', async t => {
  const foobarManifest = {
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
        'package.json': JSON.stringify(foobarManifest),
      },
    },
  })
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['@foo/bar', { name: '@foo/bar', version: '1.2.3' }]]),
  }
  MANIFESTS['@foo/bar'] = foobarManifest
  await exec(['@foo/bar'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ package: ['@foo/bar'], path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'baz' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH: process.env.PATH },
    stdio: 'inherit',
  }])
})

t.test('npm exec @foo/bar, with different bin alias and no unscoped named bin, locally installed', t => {
  const path = t.testdir()
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['@foo/bar', { name: '@foo/bar', version: '1.2.3' }]]),
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
  return t.rejects(exec(['@foo/bar'], er => {
    if (er)
      throw er
  }), {
    message: 'could not determine executable to run',
    pkgid: '@foo/bar@1.2.3',
  })
})

t.test('run command with 2 packages, need install, verify sort', t => {
  // test both directions, should use same install dir both times
  // also test the read() call here, verify that the prompts match
  const cases = [['foo', 'bar'], ['bar', 'foo']]
  t.plan(cases.length)
  for (const packages of cases) {
    t.test(packages.join(', '), async t => {
      npm.flatOptions.package = packages
      const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b))
      const path = t.testdir()
      const installDir = resolve('cache-dir/_npx/07de77790e5f40f2')
      npm.localPrefix = path
      ARB_ACTUAL_TREE[path] = {
        children: new Map(),
      }
      ARB_ACTUAL_TREE[installDir] = {
        children: new Map(),
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
      await exec(['foobar'], er => {
        if (er)
          throw er
      })
      t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
      t.match(ARB_CTOR, [{ package: packages, path }])
      t.match(ARB_REIFY, [{add, legacyPeerDeps: false}], 'need to install both packages')
      t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
      const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
      t.match(RUN_SCRIPTS, [{
        pkg: { scripts: { npx: 'foobar' } },
        banner: false,
        path: process.cwd(),
        stdioString: true,
        event: 'npx',
        env: { PATH },
        stdio: 'inherit',
      }])
    })
  }
})

t.test('npm exec foo, no bin in package', t => {
  const path = t.testdir()
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    _from: 'foo@',
    _id: 'foo@1.2.3',
  }
  return t.rejects(exec(['foo'], er => {
    if (er)
      throw er
  }), {
    message: 'could not determine executable to run',
    pkgid: 'foo@1.2.3',
  })
})

t.test('npm exec foo, many bins in package, none named foo', t => {
  const path = t.testdir()
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
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
  return t.rejects(exec(['foo'], er => {
    if (er)
      throw er
  }), {
    message: 'could not determine executable to run',
    pkgid: 'foo@1.2.3',
  })
})

t.test('npm exec -p foo -c "ls -laF"', async t => {
  const path = t.testdir()
  npm.localPrefix = path
  npm.flatOptions.package = ['foo']
  npm.flatOptions.call = 'ls -laF'
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    _from: 'foo@',
  }
  await exec([], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [], 'no need to make any dirs')
  t.match(ARB_CTOR, [{ package: ['foo'], path }])
  t.strictSame(ARB_REIFY, [], 'no need to reify anything')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'ls -laF' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH: process.env.PATH },
    stdio: 'inherit',
  }])
})

t.test('positional args and --call together is an error', t => {
  npm.flatOptions.call = 'true'
  return exec(['foo'], er => t.equal(er, exec.usage))
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

  npm.flatOptions.package = packages
  npm.flatOptions.yes = undefined

  const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b))
  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
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
  await exec(['foobar'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: packages, path }])
  t.match(ARB_REIFY, [{add, legacyPeerDeps: false}], 'need to install both packages')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foobar' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH },
    stdio: 'inherit',
  }])
  t.strictSame(READ, [{
    prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
    default: 'y',
  }])
})

t.test('skip prompt when installs are needed if not already present and shell is not a tty (multiple packages)', async t => {
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

  npm.flatOptions.package = packages
  npm.flatOptions.yes = undefined

  const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b))
  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
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
  await exec(['foobar'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: packages, path }])
  t.match(ARB_REIFY, [{add, legacyPeerDeps: false}], 'need to install both packages')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foobar' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH },
    stdio: 'inherit',
  }])
  t.strictSame(READ, [], 'should not have prompted')
  t.strictSame(LOG_WARN, [['exec', 'The following packages were not found and will be installed: bar, foo']], 'should have printed a warning')
})

t.test('skip prompt when installs are needed if not already present and shell is not a tty (single package)', async t => {
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

  npm.flatOptions.package = packages
  npm.flatOptions.yes = undefined

  const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b))
  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/f7fbba6e0636f890')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  await exec(['foobar'], er => {
    if (er)
      throw er
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: packages, path }])
  t.match(ARB_REIFY, [{add, legacyPeerDeps: false}], 'need to install the package')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
  t.match(RUN_SCRIPTS, [{
    pkg: { scripts: { npx: 'foobar' } },
    banner: false,
    path: process.cwd(),
    stdioString: true,
    event: 'npx',
    env: { PATH },
    stdio: 'inherit',
  }])
  t.strictSame(READ, [], 'should not have prompted')
  t.strictSame(LOG_WARN, [['exec', 'The following package was not found and will be installed: foo']], 'should have printed a warning')
})

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

  npm.flatOptions.package = packages
  npm.flatOptions.yes = undefined

  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
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
  await exec(['foobar'], er => {
    t.equal(er, 'canceled', 'should be canceled')
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: packages, path }])
  t.strictSame(ARB_REIFY, [], 'no install performed')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.strictSame(RUN_SCRIPTS, [])
  t.strictSame(READ, [{
    prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
    default: 'y',
  }])
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

  npm.flatOptions.package = packages
  npm.flatOptions.yes = undefined

  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
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
  await exec(['foobar'], er => {
    t.equal(er, 'canceled', 'should be canceled')
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: packages, path }])
  t.strictSame(ARB_REIFY, [], 'no install performed')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.strictSame(RUN_SCRIPTS, [])
  t.strictSame(READ, [{
    prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
    default: 'y',
  }])
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

  npm.flatOptions.package = packages
  npm.flatOptions.yes = false

  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/07de77790e5f40f2')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
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
  await exec(['foobar'], er => {
    t.equal(er, 'canceled', 'should be canceled')
  })
  t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
  t.match(ARB_CTOR, [{ package: packages, path }])
  t.strictSame(ARB_REIFY, [], 'no install performed')
  t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
  t.strictSame(RUN_SCRIPTS, [])
  t.strictSame(READ, [])
})

t.test('forward legacyPeerDeps opt', async t => {
  const path = t.testdir()
  const installDir = resolve('cache-dir/_npx/f7fbba6e0636f890')
  npm.localPrefix = path
  ARB_ACTUAL_TREE[path] = {
    children: new Map(),
  }
  ARB_ACTUAL_TREE[installDir] = {
    children: new Map(),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    bin: {
      foo: 'foo',
    },
    _from: 'foo@',
  }
  npm.flatOptions.yes = true
  npm.flatOptions.legacyPeerDeps = true
  await exec(['foo'], er => {
    if (er)
      throw er
  })
  t.match(ARB_REIFY, [{add: ['foo@'], legacyPeerDeps: true}], 'need to install foo@ using legacyPeerDeps opt')
})
