const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')
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
})

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
  libnpmexec: t.mock('libnpmexec', {
    '@npmcli/arborist': Arborist,
    '@npmcli/run-script': runScript,
    '@npmcli/ci-detect': () => CI_NAME,
    pacote,
    read,
    'mkdirp-infer-owner': mkdirp,
  }),
}
const Exec = t.mock('../../lib/exec.js', mocks)
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
  config.color = false
  config['script-shell'] = 'shell-cmd'
  config.package = []
  flatOptions.package = []
  config.call = ''
  config.yes = true
  npm.localBin = 'local-bin'
  npm.globalBin = 'global-bin'
})

t.test('npx foo, bin already exists locally', t => {
  const path = t.testdir({
    node_modules: {
      '.bin': {
        foo: 'just some file',
      },
    },
  })

  PROGRESS_IGNORED = true
  npm.localBin = resolve(path, 'node_modules', '.bin')

  exec.exec(['foo', 'one arg', 'two arg'], er => {
    t.error(er, 'npm exec')
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'foo' }},
      args: ['one arg', 'two arg'],
      cache: flatOptions.cache,
      npxCache: flatOptions.npxCache,
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: {
        PATH: [npm.localBin, ...PATH].join(delimiter),
      },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npx foo, bin already exists globally', t => {
  const path = t.testdir({
    node_modules: {
      '.bin': {
        foo: 'just some file',
      },
    },
  })

  PROGRESS_IGNORED = true
  npm.globalBin = resolve(path, 'node_modules', '.bin')

  exec.exec(['foo', 'one arg', 'two arg'], er => {
    t.error(er, 'npm exec')
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'foo' }},
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: {
        PATH: [npm.globalBin, ...PATH].join(delimiter),
      },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npm exec foo, already present locally', t => {
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
  exec.exec(['foo', 'one arg', 'two arg'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no need to reify anything')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npm exec <noargs>, run interactive shell', t => {
  CI_NAME = null
  const { isTTY } = process.stdin
  process.stdin.isTTY = true
  t.teardown(() => process.stdin.isTTY = isTTY)

  const run = (t, doRun, cb) => {
    LOG_WARN.length = 0
    ARB_CTOR.length = 0
    MKDIRPS.length = 0
    ARB_REIFY.length = 0
    npm._mockOutputs.length = 0
    exec.exec([], er => {
      if (er)
        throw er
      t.strictSame(MKDIRPS, [], 'no need to make any dirs')
      t.strictSame(ARB_CTOR, [], 'no need to instantiate arborist')
      t.strictSame(ARB_REIFY, [], 'no need to reify anything')
      t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
      if (doRun) {
        t.match(RUN_SCRIPTS, [{
          pkg: { scripts: { npx: 'shell-cmd' } },
          args: [],
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
      cb()
    })
  }
  t.test('print message when tty and not in CI', t => {
    CI_NAME = null
    process.stdin.isTTY = true
    run(t, true, () => {
      t.strictSame(LOG_WARN, [])
      t.strictSame(npm._mockOutputs, [
        [`\nEntering npm script environment at location:\n${process.cwd()}\nType 'exit' or ^D when finished\n`],
      ], 'printed message about interactive shell')
      t.end()
    })
  })

  t.test('print message with color when tty and not in CI', t => {
    CI_NAME = null
    process.stdin.isTTY = true
    config.color = true

    run(t, true, () => {
      t.strictSame(LOG_WARN, [])
      t.strictSame(npm._mockOutputs, [
        [`\u001b[0m\u001b[0m\n\u001b[0mEntering npm script environment\u001b[0m\u001b[0m at location:\u001b[0m\n\u001b[0m\u001b[2m${process.cwd()}\u001b[22m\u001b[0m\u001b[1m\u001b[22m\n\u001b[1mType 'exit' or ^D when finished\u001b[22m\n\u001b[1m\u001b[22m`],
      ], 'printed message about interactive shell')
      t.end()
    })
  })

  t.test('no message when not TTY', t => {
    CI_NAME = null
    process.stdin.isTTY = false
    run(t, true, () => {
      t.strictSame(LOG_WARN, [])
      t.strictSame(npm._mockOutputs, [], 'no message about interactive shell')
      t.end()
    })
  })

  t.test('print warning when in CI and interactive', t => {
    CI_NAME = 'travis-ci'
    process.stdin.isTTY = true
    run(t, false, () => {
      t.strictSame(LOG_WARN, [
        ['exec', 'Interactive mode disabled in CI environment'],
      ])
      t.strictSame(npm._mockOutputs, [], 'no message about interactive shell')
      t.end()
    })
  })

  t.test('not defined script-shell config value', t => {
    CI_NAME = null
    process.stdin.isTTY = true
    config['script-shell'] = undefined

    exec.exec([], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [{
        pkg: { scripts: { npx: /sh|cmd/ } },
      }])

      LOG_WARN.length = 0
      ARB_CTOR.length = 0
      MKDIRPS.length = 0
      ARB_REIFY.length = 0
      npm._mockOutputs.length = 0
      RUN_SCRIPTS.length = 0
      t.end()
    })
  })

  t.end()
})

t.test('npm exec foo, not present locally or in central loc', t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
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
  exec.exec(['foo', 'one arg', 'two arg'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.match(ARB_REIFY, [{add: ['foo@'], legacyPeerDeps: false}], 'need to install foo@')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npm exec foo, not present locally but in central loc', t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
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
  exec.exec(['foo', 'one arg', 'two arg'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.match(ARB_REIFY, [], 'no need to install again, already there')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npm exec foo, present locally but wrong version', t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/2badf4630f1cfaad')
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
  exec.exec(['foo@2.x', 'one arg', 'two arg'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.match(ARB_REIFY, [{ add: ['foo@2.x'], legacyPeerDeps: false }], 'need to add foo@2.x')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'foo' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npm exec --package=foo bar', t => {
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
  config.package = ['foo']
  flatOptions.package = ['foo']
  exec.exec(['bar', 'one arg', 'two arg'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no need to reify anything')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'bar' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npm exec @foo/bar -- --some=arg, locally installed', t => {
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
  exec.exec(['@foo/bar', '--some=arg'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no need to reify anything')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'bar' } },
      args: ['--some=arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    }])
    t.end()
  })
})

t.test('npm exec @foo/bar, with same bin alias and no unscoped named bin, locally installed', t => {
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
  exec.exec(['@foo/bar', 'one arg', 'two arg'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no need to reify anything')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.match(RUN_SCRIPTS, [{
      pkg: { scripts: { npx: 'baz' } },
      args: ['one arg', 'two arg'],
      banner: false,
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      env: { PATH: process.env.PATH },
      stdio: 'inherit',
    }])
    t.end()
  })
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
  exec.exec(['@foo/bar'], er => {
    t.match(er, {
      message: 'could not determine executable to run',
      pkgid: '@foo/bar@1.2.3',
    })
    t.end()
  })
})

t.test('run command with 2 packages, need install, verify sort', t => {
  // test both directions, should use same install dir both times
  // also test the read() call here, verify that the prompts match
  const cases = [['foo', 'bar'], ['bar', 'foo']]
  t.plan(cases.length)
  for (const packages of cases) {
    t.test(packages.join(', '), t => {
      config.package = packages
      const add = packages.map(p => `${p}@`).sort((a, b) => a.localeCompare(b, 'en'))
      const path = t.testdir()
      const installDir = resolve('npx-cache-dir/07de77790e5f40f2')
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
      exec.exec(['foobar', 'one arg', 'two arg'], er => {
        if (er)
          throw er
        t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
        t.match(ARB_CTOR, [{ path }])
        t.match(ARB_REIFY, [{add, legacyPeerDeps: false}], 'need to install both packages')
        t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
        const PATH = `${resolve(installDir, 'node_modules', '.bin')}${delimiter}${process.env.PATH}`
        t.match(RUN_SCRIPTS, [{
          pkg: { scripts: { npx: 'foobar' } },
          args: ['one arg', 'two arg'],
          banner: false,
          path: process.cwd(),
          stdioString: true,
          event: 'npx',
          env: { PATH },
          stdio: 'inherit',
        }])
        t.end()
      })
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
  exec.exec(['foo'], er => {
    t.match(er, {
      message: 'could not determine executable to run',
      pkgid: 'foo@1.2.3',
    })
    t.end()
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
  exec.exec(['foo'], er => {
    t.match(er, {
      message: 'could not determine executable to run',
      pkgid: 'foo@1.2.3',
    })
    t.end()
  })
})

t.test('npm exec -p foo -c "ls -laF"', t => {
  const path = t.testdir()
  npm.localPrefix = path
  config.package = ['foo']
  config.call = 'ls -laF'
  ARB_ACTUAL_TREE[path] = {
    children: new Map([['foo', { name: 'foo', version: '1.2.3' }]]),
  }
  MANIFESTS.foo = {
    name: 'foo',
    version: '1.2.3',
    _from: 'foo@',
  }
  exec.exec([], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [], 'no need to make any dirs')
    t.match(ARB_CTOR, [{ path }])
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
    t.end()
  })
})

t.test('positional args and --call together is an error', t => {
  config.call = 'true'
  exec.exec(['foo'], er => {
    t.equal(er, exec.usage)
    t.end()
  })
})

t.test('prompt when installs are needed if not already present and shell is a TTY', t => {
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
  exec.exec(['foobar'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
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
    t.end()
  })
})

t.test('skip prompt when installs are needed if not already present and shell is not a tty (multiple packages)', t => {
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
  exec.exec(['foobar'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
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
    t.end()
  })
})

t.test('skip prompt when installs are needed if not already present and shell is not a tty (single package)', t => {
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
  exec.exec(['foobar'], er => {
    if (er)
      throw er
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
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
    t.end()
  })
})

t.test('abort if prompt rejected', t => {
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
  exec.exec(['foobar'], er => {
    t.match(er, /canceled/, 'should be canceled')
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no install performed')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.strictSame(RUN_SCRIPTS, [])
    t.strictSame(READ, [{
      prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
      default: 'y',
    }])
    t.end()
  })
})

t.test('abort if prompt false', t => {
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
  exec.exec(['foobar'], er => {
    t.equal(er, 'canceled', 'should be canceled')
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no install performed')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.strictSame(RUN_SCRIPTS, [])
    t.strictSame(READ, [{
      prompt: 'Need to install the following packages:\n  bar\n  foo\nOk to proceed? ',
      default: 'y',
    }])
    t.end()
  })
})

t.test('abort if -n provided', t => {
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
  exec.exec(['foobar'], er => {
    t.match(er, /canceled/, 'should be canceled')
    t.strictSame(MKDIRPS, [installDir], 'need to make install dir')
    t.match(ARB_CTOR, [{ path }])
    t.strictSame(ARB_REIFY, [], 'no install performed')
    t.equal(PROGRESS_ENABLED, true, 'progress re-enabled')
    t.strictSame(RUN_SCRIPTS, [])
    t.strictSame(READ, [])
    t.end()
  })
})

t.test('forward legacyPeerDeps opt', t => {
  const path = t.testdir()
  const installDir = resolve('npx-cache-dir/f7fbba6e0636f890')
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
  config.yes = true
  flatOptions.legacyPeerDeps = true
  exec.exec(['foo'], er => {
    if (er)
      throw er
    t.match(ARB_REIFY, [{add: ['foo@'], legacyPeerDeps: true}], 'need to install foo@ using legacyPeerDeps opt')
    t.end()
  })
})

t.test('workspaces', t => {
  npm.localPrefix = t.testdir({
    node_modules: {
      '.bin': {
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

  PROGRESS_IGNORED = true
  npm.localBin = resolve(npm.localPrefix, 'node_modules/.bin')

  t.test('with args, run scripts in the context of a workspace', t => {
    exec.execWorkspaces(['foo', 'one arg', 'two arg'], ['a', 'b'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [{
        pkg: { scripts: { npx: 'foo' }},
        args: ['one arg', 'two arg'],
        banner: false,
        path: process.cwd(),
        stdioString: true,
        event: 'npx',
        env: {
          PATH: [npm.localBin, ...PATH].join(delimiter),
        },
        stdio: 'inherit',
      }])
      t.end()
    })
  })

  t.test('no args, spawn interactive shell', async t => {
    CI_NAME = null
    process.stdin.isTTY = true

    await new Promise((res, rej) => {
      exec.execWorkspaces([], ['a'], er => {
        if (er)
          return rej(er)

        t.strictSame(LOG_WARN, [])
        t.strictSame(npm._mockOutputs, [
          [`\nEntering npm script environment in workspace a@1.0.0 at location:\n${resolve(npm.localPrefix, 'packages/a')}\nType 'exit' or ^D when finished\n`],
        ], 'printed message about interactive shell')
        res()
      })
    })

    config.color = true
    npm._mockOutputs.length = 0
    await new Promise((res, rej) => {
      exec.execWorkspaces([], ['a'], er => {
        if (er)
          return rej(er)

        t.strictSame(LOG_WARN, [])
        t.strictSame(npm._mockOutputs, [
          [`\u001b[0m\u001b[0m\n\u001b[0mEntering npm script environment\u001b[0m\u001b[0m in workspace \u001b[32ma@1.0.0\u001b[39m at location:\u001b[0m\n\u001b[0m\u001b[2m${resolve(npm.localPrefix, 'packages/a')}\u001b[22m\u001b[0m\u001b[1m\u001b[22m\n\u001b[1mType 'exit' or ^D when finished\u001b[22m\n\u001b[1m\u001b[22m`],
        ], 'printed message about interactive shell')
        res()
      })
    })
  })

  t.end()
})
