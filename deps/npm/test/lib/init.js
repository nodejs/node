const t = require('tap')
const fs = require('fs')
const { resolve } = require('path')
const { fake: mockNpm } = require('../fixtures/mock-npm')

const npmLog = {
  disableProgress: () => null,
  enableProgress: () => null,
  info: () => null,
  pause: () => null,
  resume: () => null,
  silly: () => null,
}
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
  log: npmLog,
})
const mocks = {
  '../../lib/utils/usage.js': () => 'usage instructions',
}
const Init = t.mock('../../lib/init.js', mocks)
const init = new Init(npm)
const _cwd = process.cwd()
const _consolelog = console.log
const noop = () => {}

t.afterEach(() => {
  config.yes = true
  config.package = undefined
  npm.log = npmLog
  process.chdir(_cwd)
  console.log = _consolelog
})

t.test('classic npm init -y', t => {
  npm.localPrefix = t.testdir({})

  // init-package-json prints directly to console.log
  // this avoids poluting test output with those logs
  console.log = noop

  process.chdir(npm.localPrefix)
  init.exec([], err => {
    if (err)
      throw err

    const pkg = require(resolve(npm.localPrefix, 'package.json'))
    t.equal(pkg.version, '1.0.0')
    t.equal(pkg.license, 'ISC')
    t.end()
  })
})

t.test('classic interactive npm init', t => {
  npm.localPrefix = t.testdir({})
  config.yes = undefined

  const Init = t.mock('../../lib/init.js', {
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
  init.exec([], err => {
    if (err)
      throw err

    t.end()
  })
})

t.test('npm init <arg>', t => {
  t.plan(3)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
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
  init.exec(['react-app'], err => {
    if (err)
      throw err
  })
})

t.test('npm init <arg> -- other-args', t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
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
  init.exec(
    ['react-app', 'my-path', '--some-option', 'some-value'],
    err => {
      if (err)
        throw err
    }
  )
})

t.test('npm init @scope/name', t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
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
  init.exec(['@npmcli/something'], err => {
    if (err)
      throw err
  })
})

t.test('npm init git spec', t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
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
  init.exec(['npm/something'], err => {
    if (err)
      throw err
  })
})

t.test('npm init @scope', t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
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
  init.exec(['@npmcli'], err => {
    if (err)
      throw err
  })
})

t.test('npm init tgz', t => {
  npm.localPrefix = t.testdir({})

  process.chdir(npm.localPrefix)
  init.exec(['something.tgz'], err => {
    t.match(
      err,
      /Error: Unrecognized initializer: something.tgz/,
      'should throw error when using an unsupported spec'
    )
    t.end()
  })
})

t.test('npm init <arg>@next', t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
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
  init.exec(['something@next'], err => {
    if (err)
      throw err
  })
})

t.test('npm init exec error', t => {
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
    libnpmexec: async ({ args }) => {
      throw new Error('ERROR')
    },
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  init.exec(['something@next'], err => {
    t.match(
      err,
      /ERROR/,
      'should exit with exec error'
    )
    t.end()
  })
})

t.test('should not rewrite flatOptions', t => {
  t.plan(1)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
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
  init.exec(['react-app', 'my-app'], err => {
    if (err)
      throw err
  })
})

t.test('npm init cancel', t => {
  t.plan(2)
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
    ...mocks,
    'init-package-json': (dir, initFile, config, cb) => cb(
      new Error('canceled')
    ),
  })
  const init = new Init(npm)
  npm.log = { ...npm.log }
  npm.log.warn = (title, msg) => {
    t.equal(title, 'init', 'should have init title')
    t.equal(msg, 'canceled', 'should log canceled')
  }

  process.chdir(npm.localPrefix)
  init.exec([], err => {
    if (err)
      throw err
  })
})

t.test('npm init error', t => {
  npm.localPrefix = t.testdir({})

  const Init = t.mock('../../lib/init.js', {
    ...mocks,
    'init-package-json': (dir, initFile, config, cb) => cb(
      new Error('Unknown Error')
    ),
  })
  const init = new Init(npm)

  process.chdir(npm.localPrefix)
  init.exec([], err => {
    t.match(err, /Unknown Error/, 'should throw error')
    t.end()
  })
})

t.test('workspaces', t => {
  t.test('no args', t => {
    t.teardown(() => {
      npm._mockOutputs.length = 0
    })
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'top-level',
      }),
    })

    const Init = t.mock('../../lib/init.js', {
      ...mocks,
      'init-package-json': (dir, initFile, config, cb) => {
        t.equal(dir, resolve(npm.localPrefix, 'a'), 'should use the ws path')
        cb()
      },
    })
    const init = new Init(npm)
    init.execWorkspaces([], ['a'], err => {
      if (err)
        throw err

      t.matchSnapshot(npm._mockOutputs, 'should print helper info')
      t.end()
    })
  })

  t.test('no args, existing folder', t => {
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

    init.execWorkspaces([], ['packages/a'], err => {
      if (err)
        throw err

      t.matchSnapshot(npm._mockOutputs, 'should print helper info')
      t.end()
    })
  })

  t.test('with arg but missing workspace folder', t => {
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

    init.execWorkspaces([], ['packages/a'], err => {
      if (err)
        throw err

      t.matchSnapshot(npm._mockOutputs, 'should print helper info')
      t.end()
    })
  })

  t.test('fail parsing top-level package.json to set workspace', t => {
    // init-package-json prints directly to console.log
    // this avoids poluting test output with those logs
    console.log = noop

    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'top-level',
      }),
    })

    const Init = t.mock('../../lib/init.js', {
      ...mocks,
      '@npmcli/package-json': {
        async load () {
          throw new Error('ERR')
        },
      },
    })
    const init = new Init(npm)

    init.execWorkspaces([], ['a'], err => {
      t.match(
        err,
        /ERR/,
        'should exit with error'
      )
      t.end()
    })
  })

  t.test('missing top-level package.json when settting workspace', t => {
    // init-package-json prints directly to console.log
    // this avoids poluting test output with those logs
    console.log = noop

    npm.localPrefix = t.testdir({})

    const Init = require('../../lib/init.js')
    const init = new Init(npm)

    init.execWorkspaces([], ['a'], err => {
      t.match(
        err,
        { code: 'ENOENT' },
        'should exit with missing package.json file error'
      )
      t.end()
    })
  })

  t.test('using args', t => {
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

    const Init = t.mock('../../lib/init.js', {
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
    init.execWorkspaces(['react-app'], ['a'], err => {
      if (err)
        throw err

      t.end()
    })
  })

  t.end()
})
