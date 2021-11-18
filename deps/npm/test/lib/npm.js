const t = require('tap')

const npmlog = require('npmlog')
const { real: mockNpm } = require('../fixtures/mock-npm.js')

// delete this so that we don't have configs from the fact that it
// is being run by 'npm test'
const event = process.env.npm_lifecycle_event

for (const env of Object.keys(process.env).filter(e => /^npm_/.test(e))) {
  if (env === 'npm_command') {
    // should only be running this in the 'test' or 'run-script' command!
    // if the lifecycle event is 'test', then it'll be either 'test' or 'run',
    // otherwise it should always be run-script. Of course, it'll be missing
    // if this test is just run directly, which is also acceptable.
    if (event === 'test') {
      t.ok(
        ['test', 'run-script'].some(i => i === event),
        'should match "npm test" or "npm run test"'
      )
    } else {
      t.match(process.env[env], /^(run-script|exec)$/)
    }
  }
  delete process.env[env]
}

const { resolve, dirname } = require('path')

const actualPlatform = process.platform
const beWindows = () => {
  Object.defineProperty(process, 'platform', {
    value: 'win32',
    configurable: true,
  })
}
const bePosix = () => {
  Object.defineProperty(process, 'platform', {
    value: 'posix',
    configurable: true,
  })
}
const argv = [...process.argv]

t.afterEach(() => {
  for (const env of Object.keys(process.env).filter(e => /^npm_/.test(e))) {
    delete process.env[env]
  }
  process.env.npm_config_cache = CACHE
  process.argv = argv
  Object.defineProperty(process, 'platform', {
    value: actualPlatform,
    configurable: true,
  })
})

const CACHE = t.testdir()
process.env.npm_config_cache = CACHE

t.test('not yet loaded', async t => {
  const { Npm, logs } = mockNpm(t)
  const npm = new Npm()
  t.match(npm, {
    started: Number,
    command: null,
    config: {
      loaded: false,
      get: Function,
      set: Function,
    },
    version: String,
    shelloutCommands: Array,
  })
  t.throws(() => npm.config.set('foo', 'bar'))
  t.throws(() => npm.config.get('foo'))
  t.same(logs, [])
  t.end()
})

t.test('npm.load', async t => {
  t.test('load error', async t => {
    const { Npm } = mockNpm(t)
    const npm = new Npm()
    const loadError = new Error('load error')
    npm.config.load = async () => {
      throw loadError
    }
    await t.rejects(
      () => npm.load(),
      /load error/
    )

    t.equal(npm.loadErr, loadError)
    npm.config.load = async () => {
      throw new Error('different error')
    }
    await t.rejects(
      () => npm.load(),
      /load error/,
      'loading again returns the original error'
    )
    t.equal(npm.loadErr, loadError)
  })

  t.test('basic loading', async t => {
    const { Npm, logs } = mockNpm(t)
    const npm = new Npm()
    const dir = t.testdir({
      node_modules: {},
    })
    await npm.load()
    t.equal(npm.loaded, true)
    t.equal(npm.config.loaded, true)
    t.equal(npm.config.get('force'), false)
    t.ok(npm.usage, 'has usage')
    npm.config.set('prefix', dir)

    t.match(npm, {
      flatOptions: {},
    })
    t.match(logs, [
      ['timing', 'npm:load', /Completed in [0-9.]+ms/],
    ])

    bePosix()
    t.equal(resolve(npm.cache), resolve(CACHE), 'cache is cache')
    const newCache = t.testdir()
    npm.cache = newCache
    t.equal(npm.config.get('cache'), newCache, 'cache setter sets config')
    t.equal(npm.cache, newCache, 'cache getter gets new config')
    t.equal(npm.log, npmlog, 'npmlog getter')
    t.equal(npm.lockfileVersion, 2, 'lockfileVersion getter')
    t.equal(npm.prefix, npm.localPrefix, 'prefix is local prefix')
    t.not(npm.prefix, npm.globalPrefix, 'prefix is not global prefix')
    npm.globalPrefix = npm.prefix
    t.equal(npm.prefix, npm.globalPrefix, 'globalPrefix setter')
    npm.localPrefix = dir + '/extra/prefix'
    t.equal(npm.prefix, npm.localPrefix, 'prefix is local prefix after localPrefix setter')
    t.not(npm.prefix, npm.globalPrefix, 'prefix is not global prefix after localPrefix setter')

    npm.prefix = dir + '/some/prefix'
    t.equal(npm.prefix, npm.localPrefix, 'prefix is local prefix after prefix setter')
    t.not(npm.prefix, npm.globalPrefix, 'prefix is not global prefix after prefix setter')
    t.equal(npm.bin, npm.localBin, 'bin is local bin after prefix setter')
    t.not(npm.bin, npm.globalBin, 'bin is not global bin after prefix setter')
    t.equal(npm.dir, npm.localDir, 'dir is local dir after prefix setter')
    t.not(npm.dir, npm.globalDir, 'dir is not global dir after prefix setter')

    npm.config.set('global', true)
    t.equal(npm.prefix, npm.globalPrefix, 'prefix is global prefix after setting global')
    t.not(npm.prefix, npm.localPrefix, 'prefix is not local prefix after setting global')
    t.equal(npm.bin, npm.globalBin, 'bin is global bin after setting global')
    t.not(npm.bin, npm.localBin, 'bin is not local bin after setting global')
    t.equal(npm.dir, npm.globalDir, 'dir is global dir after setting global')
    t.not(npm.dir, npm.localDir, 'dir is not local dir after setting global')

    npm.prefix = dir + '/new/global/prefix'
    t.equal(npm.prefix, npm.globalPrefix, 'prefix is global prefix after prefix setter')
    t.not(npm.prefix, npm.localPrefix, 'prefix is not local prefix after prefix setter')
    t.equal(npm.bin, npm.globalBin, 'bin is global bin after prefix setter')
    t.not(npm.bin, npm.localBin, 'bin is not local bin after prefix setter')

    beWindows()
    t.equal(npm.bin, npm.globalBin, 'bin is global bin in windows mode')
    t.equal(npm.dir, npm.globalDir, 'dir is global dir in windows mode')
    bePosix()

    const tmp = npm.tmp
    t.match(tmp, String, 'npm.tmp is a string')
    t.equal(tmp, npm.tmp, 'getter only generates it once')
  })

  t.test('forceful loading', async t => {
    process.argv = [...process.argv, '--force', '--color', 'always']
    const { Npm, logs } = mockNpm(t)
    const npm = new Npm()
    await npm.load()
    t.match(logs.filter(l => l[0] !== 'timing'), [
      [
        'warn',
        'using --force',
        'Recommended protections disabled.',
      ],
    ])
  })

  t.test('node is a symlink', async t => {
    const node = actualPlatform === 'win32' ? 'node.exe' : 'node'
    const dir = t.testdir({
      '.npmrc': 'foo = bar',
      bin: t.fixture('symlink', dirname(process.execPath)),
    })

    const PATH = process.env.PATH || process.env.Path
    process.env.PATH = resolve(dir, 'bin')
    process.argv = [
      node,
      process.argv[1],
      '--prefix', dir,
      '--userconfig', `${dir}/.npmrc`,
      '--usage',
      '--scope=foo',
      'token',
      'revoke',
      'blergggg',
    ]

    t.teardown(() => {
      process.env.PATH = PATH
    })

    const { Npm, logs, outputs } = mockNpm(t)
    const npm = new Npm()
    await npm.load()
    t.equal(npm.config.get('scope'), '@foo', 'added the @ sign to scope')
    t.match(logs.filter(l => l[0] !== 'timing' || !/^config:/.test(l[1])), [
      [
        'timing',
        'npm:load:whichnode',
        /Completed in [0-9.]+ms/,
      ],
      [
        'verbose',
        'node symlink',
        resolve(dir, 'bin', node),
      ],
      [
        'timing',
        'npm:load',
        /Completed in [0-9.]+ms/,
      ],
    ])
    t.equal(process.execPath, resolve(dir, 'bin', node))

    outputs.length = 0
    await npm.exec('ll', [])

    t.equal(npm.command, 'll', 'command set to first npm command')
    t.equal(npm.flatOptions.npmCommand, 'll', 'npmCommand flatOption set')

    const ll = await npm.cmd('ll')
    t.same(outputs, [[ll.usage]], 'print usage')
    npm.config.set('usage', false)

    outputs.length = 0
    logs.length = 0
    await npm.exec('get', ['scope', '\u2010not-a-dash'])

    t.strictSame([npm.command, npm.flatOptions.npmCommand], ['ll', 'll'],
      'does not change npm.command when another command is called')

    t.match(logs, [
      [
        'error',
        'arg',
        'Argument starts with non-ascii dash, this is probably invalid:',
        '\u2010not-a-dash',
      ],
      [
        'timing',
        'command:config',
        /Completed in [0-9.]+ms/,
      ],
      [
        'timing',
        'command:get',
        /Completed in [0-9.]+ms/,
      ],
    ])
    t.same(outputs, [['scope=@foo\n\u2010not-a-dash=undefined']])
  })

  t.test('--no-workspaces with --workspace', async t => {
    const dir = t.testdir({
      packages: {
        a: {
          'package.json': JSON.stringify({
            name: 'a',
            version: '1.0.0',
            scripts: { test: 'echo test a' },
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'root',
        version: '1.0.0',
        workspaces: ['./packages/*'],
      }),
    })
    process.argv = [
      process.execPath,
      process.argv[1],
      '--userconfig', resolve(dir, '.npmrc'),
      '--color', 'false',
      '--workspaces', 'false',
      '--workspace', 'a',
    ]
    const { Npm } = mockNpm(t)
    const npm = new Npm()
    npm.localPrefix = dir
    await t.rejects(
      npm.exec('run', []),
      /Can not use --no-workspaces and --workspace at the same time/
    )
  })

  t.test('workspace-aware configs and commands', async t => {
    const dir = t.testdir({
      packages: {
        a: {
          'package.json': JSON.stringify({
            name: 'a',
            version: '1.0.0',
            scripts: { test: 'echo test a' },
          }),
        },
        b: {
          'package.json': JSON.stringify({
            name: 'b',
            version: '1.0.0',
            scripts: { test: 'echo test b' },
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'root',
        version: '1.0.0',
        workspaces: ['./packages/*'],
      }),
      '.npmrc': '',
    })

    process.argv = [
      process.execPath,
      process.argv[1],
      '--userconfig',
      resolve(dir, '.npmrc'),
      '--color',
      'false',
      '--workspaces',
      'true',
    ]

    const { Npm, outputs } = mockNpm(t)
    const npm = new Npm()
    await npm.load()
    npm.localPrefix = dir

    // verify that calling the command with a short name still sets
    // the npm.command property to the full canonical name of the cmd.
    npm.command = null
    await npm.exec('run', [])

    t.equal(npm.command, 'run-script', 'npm.command set to canonical name')

    t.match(
      outputs,
      [
        ['Lifecycle scripts included in a@1.0.0:'],
        ['  test\n    echo test a'],
        [''],
        ['Lifecycle scripts included in b@1.0.0:'],
        ['  test\n    echo test b'],
        [''],
      ],
      'should exec workspaces version of commands'
    )
  })

  t.test('workspaces in global mode', async t => {
    const dir = t.testdir({
      packages: {
        a: {
          'package.json': JSON.stringify({
            name: 'a',
            version: '1.0.0',
            scripts: { test: 'echo test a' },
          }),
        },
        b: {
          'package.json': JSON.stringify({
            name: 'b',
            version: '1.0.0',
            scripts: { test: 'echo test b' },
          }),
        },
      },
      'package.json': JSON.stringify({
        name: 'root',
        version: '1.0.0',
        workspaces: ['./packages/*'],
      }),
    })
    process.argv = [
      process.execPath,
      process.argv[1],
      '--userconfig',
      resolve(dir, '.npmrc'),
      '--color',
      'false',
      '--workspaces',
      '--global',
      'true',
    ]
    const { Npm } = mockNpm(t)
    const npm = new Npm()
    await npm.load()
    npm.localPrefix = dir
    // verify that calling the command with a short name still sets
    // the npm.command property to the full canonical name of the cmd.
    npm.command = null
    await t.rejects(
      npm.exec('run', []),
      /Workspaces not supported for global packages/
    )
  })
})

t.test('set process.title', async t => {
  t.test('basic title setting', async t => {
    process.argv = [
      process.execPath,
      process.argv[1],
      '--usage',
      '--scope=foo',
      'ls',
    ]
    const { Npm } = mockNpm(t)
    const npm = new Npm()
    await npm.load()
    t.equal(npm.title, 'npm ls')
    t.equal(process.title, 'npm ls')
  })

  t.test('do not expose token being revoked', async t => {
    process.argv = [
      process.execPath,
      process.argv[1],
      '--usage',
      '--scope=foo',
      'token',
      'revoke',
      'deadbeefcafebad',
    ]
    const { Npm } = mockNpm(t)
    const npm = new Npm()
    await npm.load()
    t.equal(npm.title, 'npm token revoke ***')
    t.equal(process.title, 'npm token revoke ***')
  })

  t.test('do show *** unless a token is actually being revoked', async t => {
    process.argv = [
      process.execPath,
      process.argv[1],
      '--usage',
      '--scope=foo',
      'token',
      'revoke',
    ]
    const { Npm } = mockNpm(t)
    const npm = new Npm()
    await npm.load()
    t.equal(npm.title, 'npm token revoke')
    t.equal(process.title, 'npm token revoke')
  })
})

t.test('timings', t => {
  const { Npm, logs } = mockNpm(t)
  const npm = new Npm()
  process.emit('time', 'foo')
  process.emit('time', 'bar')
  t.match(npm.timers.get('foo'), Number, 'foo timer is a number')
  t.match(npm.timers.get('bar'), Number, 'foo timer is a number')
  process.emit('timeEnd', 'foo')
  process.emit('timeEnd', 'bar')
  process.emit('timeEnd', 'baz')
  t.match(logs, [
    ['timing', 'foo', /Completed in [0-9]+ms/],
    ['timing', 'bar', /Completed in [0-9]+ms/],
    [
      'silly',
      'timing',
      "Tried to end timer that doesn't exist:",
      'baz',
    ],
  ])
  t.notOk(npm.timers.has('foo'), 'foo timer is gone')
  t.notOk(npm.timers.has('bar'), 'bar timer is gone')
  t.match(npm.timings, { foo: Number, bar: Number })
  t.end()
})

t.test('output clears progress and console.logs the message', t => {
  const mock = mockNpm(t)
  const { Npm, logs } = mock
  const npm = new Npm()
  npm.output = mock.npmOutput
  const { log } = console
  const { log: { clearProgress, showProgress } } = npm
  let showingProgress = true
  npm.log.clearProgress = () => showingProgress = false
  npm.log.showProgress = () => showingProgress = true
  console.log = (...args) => {
    t.equal(showingProgress, false, 'should not be showing progress right now')
    logs.push(args)
  }
  t.teardown(() => {
    console.log = log
    npm.log.showProgress = showProgress
    npm.log.clearProgress = clearProgress
  })

  npm.output('hello')
  t.strictSame(logs, [['hello']])
  t.end()
})

t.test('unknown command', async t => {
  const mock = mockNpm(t)
  const { Npm } = mock
  const npm = new Npm()
  await t.rejects(
    npm.cmd('thisisnotacommand'),
    { code: 'EUNKNOWNCOMMAND' }
  )
})
