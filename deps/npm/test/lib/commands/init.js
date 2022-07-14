const t = require('tap')
const fs = require('fs')
const { resolve } = require('path')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const config = {
  cache: 'bad-cache-dir',
  'init-module': '~/.npm-init.js',
  yes: true,
}
const flatOptions = {
  cache: 'test-config-dir/_cacache',
  npxCache: 'test-config-dir/_npx',
}
const npm = mockNpm({
  flatOptions,
  config,
})
const mocks = {
  npmlog: {
    disableProgress: () => null,
    enableProgress: () => null,
  },
  'proc-log': {
    info: () => null,
    pause: () => null,
    resume: () => null,
    silly: () => null,
  },
}
const Init = t.mock('../../../lib/commands/init.js', mocks)
const init = new Init(npm)
const _cwd = process.cwd()
const _consolelog = console.log
const noop = () => {}

t.afterEach(() => {
  config.yes = true
  config.package = undefined
  process.chdir(_cwd)
  console.log = _consolelog
})

t.test('classic npm init -y', async t => {
  npm.localPrefix = t.testdir({})

  // init-package-json prints directly to console.log
  // this avoids poluting test output with those logs
  console.log = noop

  process.chdir(npm.localPrefix)
  await init.exec([])

  const pkg = require(resolve(npm.localPrefix, 'package.json'))
  t.equal(pkg.version, '1.0.0')
  t.equal(pkg.license, 'ISC')
})

t.test('classic interactive npm init', async t => {
  npm.localPrefix = t.testdir({})
  config.yes = undefined

  const Init = t.mock('../../../lib/commands/init.js', {
    ...mocks,
    'init-package-json': (path, initFile, config, cb) => {
      t.equal(
        path,
        resolve(npm.localPrefix),
        'should start init package.json in expected path'
      )
      cb()
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec([])
})

t.test('npm init <arg>', async t => {
  t.plan(3)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: ({ args, cache, npxCache }) => {
      t.same(
        args,
        ['create-react-app'],
        'should npx with listed packages'
      )
      t.same(cache, flatOptions.cache)
      t.same(npxCache, flatOptions.npxCache)
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec(['react-app'])
})

t.test('npm init <arg> -- other-args', async t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: ({ args }) => {
      t.same(
        args,
        ['create-react-app', 'my-path', '--some-option', 'some-value'],
        'should npm exec with expected args'
      )
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec(['react-app', 'my-path', '--some-option', 'some-value'])
})

t.test('npm init @scope/name', async t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: ({ args }) => {
      t.same(
        args,
        ['@npmcli/create-something'],
        'should npx with scoped packages'
      )
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec(['@npmcli/something'])
})

t.test('npm init git spec', async t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: ({ args }) => {
      t.same(
        args,
        ['npm/create-something'],
        'should npx with git-spec packages'
      )
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec(['npm/something'])
})

t.test('npm init @scope', async t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: ({ args }) => {
      t.same(
        args,
        ['@npmcli/create'],
        'should npx with @scope/create pkgs'
      )
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec(['@npmcli'])
})

t.test('npm init tgz', async t => {
  npm.localPrefix = t.testdir({})

  process.chdir(npm.localPrefix)
  await t.rejects(
    init.exec(['something.tgz']),
    /Unrecognized initializer: something.tgz/,
    'should throw error when using an unsupported spec'
  )
})

t.test('npm init <arg>@next', async t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: ({ args }) => {
      t.same(
        args,
        ['create-something@next'],
        'should npx with something@next'
      )
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec(['something@next'])
})

t.test('npm init exec error', async t => {
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: async ({ args }) => {
      throw new Error('ERROR')
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await t.rejects(
    init.exec(['something@next']),
    /ERROR/,
    'should exit with exec error'
  )
})

t.test('should not rewrite flatOptions', async t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    libnpmexec: async ({ args }) => {
      t.same(
        args,
        ['create-react-app', 'my-app'],
        'should npx with extra args'
      )
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec(['react-app', 'my-app'])
})

t.test('npm init cancel', async t => {
  t.plan(2)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    ...mocks,
    'init-package-json': (dir, initFile, config, cb) => cb(
      new Error('canceled')
    ),
    'proc-log': {
      ...mocks['proc-log'],
      warn: (title, msg) => {
        t.equal(title, 'init', 'should have init title')
        t.equal(msg, 'canceled', 'should log canceled')
      },
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await init.exec([])
})

t.test('npm init error', async t => {
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../../lib/commands/init.js', {
    ...mocks,
    'init-package-json': (dir, initFile, config, cb) => cb(
      new Error('Unknown Error')
    ),
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  await t.rejects(
    init.exec([]),
    /Unknown Error/,
    'should throw error'
  )
})

t.test('workspaces', t => {
  t.test('no args', async t => {
    t.teardown(() => {
      npm._mockOutputs.length = 0
    })
    npm._mockOutputs.length = 0
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'top-level',
      }),
    })

    const Init = t.mock('../../../lib/commands/init.js', {
      ...mocks,
      'init-package-json': (dir, initFile, config, cb) => {
        t.equal(dir, resolve(npm.localPrefix, 'a'), 'should use the ws path')
        cb()
      },
    })
    const init = new Init(npm)
    await init.execWorkspaces([], ['a'])
    t.matchSnapshot(npm._mockOutputs, 'should print helper info')
  })

  t.test('post workspace-init reify', async t => {
    const _consolelog = console.log
    console.log = () => null
    t.teardown(() => {
      console.log = _consolelog
      npm._mockOutputs.length = 0
      delete npm.flatOptions.workspacesUpdate
    })
    npm.started = Date.now()
    npm._mockOutputs.length = 0
    npm.flatOptions.workspacesUpdate = true
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'top-level',
      }),
    })

    const Init = t.mock('../../../lib/commands/init.js', {
      ...mocks,
      'init-package-json': (dir, initFile, config, cb) => {
        t.equal(dir, resolve(npm.localPrefix, 'a'), 'should use the ws path')
        return require('init-package-json')(dir, initFile, config, cb)
      },
    })
    const init = new Init(npm)
    await init.execWorkspaces([], ['a'])
    const output = npm._mockOutputs.map(arr => arr.map(i => i.replace(/[0-9]*ms$/, '100ms')))
    t.matchSnapshot(output, 'should print helper info')
    const lockFilePath = resolve(npm.localPrefix, 'package-lock.json')
    const lockFile = fs.readFileSync(lockFilePath, { encoding: 'utf8' })
    t.matchSnapshot(lockFile, 'should reify tree on init ws complete')
  })

  t.test('no args, existing folder', async t => {
    t.teardown(() => {
      npm._mockOutputs.length = 0
    })
    // init-package-json prints directly to console.log
    // this avoids poluting test output with those logs
    console.log = noop

    npm.localPrefix = t.testdir({
      packages: {
        a: {
          'package.json': JSON.stringify({
            name: 'a',
            version: '1.0.0',
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'top-level',
        workspaces: ['packages/a'],
      }),
    })

    await init.execWorkspaces([], ['packages/a'])

    t.matchSnapshot(npm._mockOutputs, 'should print helper info')
  })

  t.test('with arg but missing workspace folder', async t => {
    t.teardown(() => {
      npm._mockOutputs.length = 0
    })
    // init-package-json prints directly to console.log
    // this avoids poluting test output with those logs
    console.log = noop

    npm.localPrefix = t.testdir({
      node_modules: {
        a: t.fixture('symlink', '../a'),
        'create-index': {
          'index.js': ``,
        },
      },
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
        }),
      },
      'package.json': JSON.stringify({
        name: 'top-level',
      }),
    })

    await init.execWorkspaces([], ['packages/a'])

    t.matchSnapshot(npm._mockOutputs, 'should print helper info')
  })

  t.test('fail parsing top-level package.json to set workspace', async t => {
    // init-package-json prints directly to console.log
    // this avoids poluting test output with those logs
    console.log = noop

    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'top-level',
      }),
    })

    const Init = t.mock('../../../lib/commands/init.js', {
      ...mocks,
      '@npmcli/package-json': {
        async load () {
          throw new Error('ERR')
        },
      },
    })
    const init = new Init(npm)

    await t.rejects(
      init.execWorkspaces([], ['a']),
      /ERR/,
      'should exit with error'
    )
  })

  t.test('missing top-level package.json when settting workspace', async t => {
    // init-package-json prints directly to console.log
    // this avoids poluting test output with those logs
    console.log = noop

    npm.localPrefix = t.testdir({})

    const Init = require('../../../lib/commands/init.js')
    const init = new Init(npm)

    await t.rejects(
      init.execWorkspaces([], ['a']),
      { code: 'ENOENT' },
      'should exit with missing package.json file error'
    )
  })

  t.test('using args', async t => {
    npm.localPrefix = t.testdir({
      b: {
        'package.json': JSON.stringify({
          name: 'b',
        }),
      },
      'package.json': JSON.stringify({
        name: 'top-level',
        workspaces: ['b'],
      }),
    })

    const Init = t.mock('../../../lib/commands/init.js', {
      ...mocks,
      libnpmexec: ({ args, path }) => {
        t.same(
          args,
          ['create-react-app'],
          'should npx with listed packages'
        )
        t.same(
          path,
          resolve(npm.localPrefix, 'a'),
          'should use workspace path'
        )
        fs.writeFileSync(
          resolve(npm.localPrefix, 'a/package.json'),
          JSON.stringify({ name: 'a' })
        )
      },
    })

    const init = new Init(npm)
    await init.execWorkspaces(['react-app'], ['a'])
  })

  t.end()
})

t.test('npm init workspces with root', async t => {
  t.teardown(() => {
    npm._mockOutputs.length = 0
  })
  npm.localPrefix = t.testdir({})
  npm.flatOptions.includeWorkspaceRoot = true

  // init-package-json prints directly to console.log
  // this avoids poluting test output with those logs
  console.log = noop

  process.chdir(npm.localPrefix)
  await init.execWorkspaces([], ['packages/a'])
  const pkg = require(resolve(npm.localPrefix, 'package.json'))
  t.equal(pkg.version, '1.0.0')
  t.equal(pkg.license, 'ISC')
  t.matchSnapshot(npm._mockOutputs, 'does not print helper info')
})
