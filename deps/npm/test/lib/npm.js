const t = require('tap')
const { resolve, dirname, join } = require('path')

const { load: loadMockNpm } = require('../fixtures/mock-npm.js')
const mockGlobals = require('../fixtures/mock-globals')

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
        ['test', 'run-script'].some(i => i === process.env[env]),
        'should match "npm test" or "npm run test"'
      )
    } else {
      t.match(process.env[env], /^(run-script|exec)$/)
    }
  }
  delete process.env[env]
}

t.afterEach(async (t) => {
  for (const env of Object.keys(process.env).filter(e => /^npm_/.test(e))) {
    delete process.env[env]
  }
})

t.test('not yet loaded', async t => {
  const { npm, logs } = await loadMockNpm(t, { load: false })
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
    const { npm } = await loadMockNpm(t, { load: false })
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
    const { npm, logs, prefix: dir, cache } = await loadMockNpm(t, {
      prefixDir: { node_modules: {} },
    })

    t.equal(npm.loaded, true)
    t.equal(npm.config.loaded, true)
    t.equal(npm.config.get('force'), false)
    t.ok(npm.usage, 'has usage')

    t.match(npm, {
      flatOptions: {},
    })
    t.match(logs.timing.filter(([p]) => p === 'npm:load'), [
      ['npm:load', /Completed in [0-9.]+ms/],
    ])

    mockGlobals(t, { process: { platform: 'posix' } })
    t.equal(resolve(npm.cache), resolve(cache), 'cache is cache')
    const newCache = t.testdir()
    npm.cache = newCache
    t.equal(npm.config.get('cache'), newCache, 'cache setter sets config')
    t.equal(npm.cache, newCache, 'cache getter gets new config')
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

    mockGlobals(t, { process: { platform: 'win32' } })
    t.equal(npm.bin, npm.globalBin, 'bin is global bin in windows mode')
    t.equal(npm.dir, npm.globalDir, 'dir is global dir in windows mode')

    const tmp = npm.tmp
    t.match(tmp, String, 'npm.tmp is a string')
    t.equal(tmp, npm.tmp, 'getter only generates it once')
  })

  t.test('forceful loading', async t => {
    mockGlobals(t, {
      'process.argv': [...process.argv, '--force', '--color', 'always'],
    })
    const { logs } = await loadMockNpm(t)
    t.match(logs.warn, [
      [
        'using --force',
        'Recommended protections disabled.',
      ],
    ])
  })

  t.test('node is a symlink', async t => {
    const node = process.platform === 'win32' ? 'node.exe' : 'node'
    mockGlobals(t, {
      'process.argv': [
        node,
        process.argv[1],
        '--usage',
        '--scope=foo',
        'token',
        'revoke',
        'blergggg',
      ],
    })
    const { npm, logs, outputs, prefix } = await loadMockNpm(t, {
      prefixDir: {
        bin: t.fixture('symlink', dirname(process.execPath)),
      },
      globals: ({ prefix }) => ({
        'process.env.PATH': resolve(prefix, 'bin'),
      }),
    })

    t.equal(npm.config.get('scope'), '@foo', 'added the @ sign to scope')
    t.match([
      ...logs.timing.filter(([p]) => p === 'npm:load:whichnode'),
      ...logs.verbose,
      ...logs.timing.filter(([p]) => p === 'npm:load'),
    ], [
      ['npm:load:whichnode', /Completed in [0-9.]+ms/],
      ['node symlink', resolve(prefix, 'bin', node)],
      ['logfile', /.*-debug-0.log/],
      ['npm:load', /Completed in [0-9.]+ms/],
    ])
    t.equal(process.execPath, resolve(prefix, 'bin', node))

    outputs.length = 0
    logs.length = 0
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
    mockGlobals(t, {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--color', 'false',
        '--workspaces', 'false',
        '--workspace', 'a',
      ],
    })
    const { npm } = await loadMockNpm(t, {
      load: false,
      prefixDir: {
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
      },
    })
    await t.rejects(
      npm.exec('run', []),
      /Can not use --no-workspaces and --workspace at the same time/
    )
  })

  t.test('workspace-aware configs and commands', async t => {
    mockGlobals(t, {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--color', 'false',
        '--workspaces', 'true',
      ],
    })
    const { npm, outputs } = await loadMockNpm(t, {
      prefixDir: {
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
      },
    })

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
    mockGlobals(t, {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--color',
        'false',
        '--workspaces',
        '--global',
        'true',
      ],
    })
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
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
      },
    })
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
    mockGlobals(t, {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--usage',
        '--scope=foo',
        'ls',
      ],
    })
    const { npm } = await loadMockNpm(t)
    t.equal(npm.title, 'npm ls')
    t.equal(process.title, 'npm ls')
  })

  t.test('do not expose token being revoked', async t => {
    mockGlobals(t, {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--usage',
        '--scope=foo',
        'token',
        'revoke',
        'deadbeefcafebad',
      ],
    })
    const { npm } = await loadMockNpm(t)
    t.equal(npm.title, 'npm token revoke ***')
    t.equal(process.title, 'npm token revoke ***')
  })

  t.test('do show *** unless a token is actually being revoked', async t => {
    mockGlobals(t, {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--usage',
        '--scope=foo',
        'token',
        'revoke',
      ],
    })
    const { npm } = await loadMockNpm(t)
    t.equal(npm.title, 'npm token revoke')
    t.equal(process.title, 'npm token revoke')
  })
})

t.test('debug-log', async t => {
  const { npm, debugFile } = await loadMockNpm(t, { load: false })

  const log1 = ['silly', 'test', 'before load']
  const log2 = ['silly', 'test', 'after load']

  process.emit('log', ...log1)
  await npm.load()
  process.emit('log', ...log2)

  const debug = await debugFile()
  t.equal(npm.logFiles.length, 1, 'one debug file')
  t.match(debug, log1.join(' '), 'before load appears')
  t.match(debug, log2.join(' '), 'after load log appears')
})

t.test('timings', async t => {
  t.test('gets/sets timers', async t => {
    const { npm, logs } = await loadMockNpm(t, { load: false })
    process.emit('time', 'foo')
    process.emit('time', 'bar')
    t.match(npm.unfinishedTimers.get('foo'), Number, 'foo timer is a number')
    t.match(npm.unfinishedTimers.get('bar'), Number, 'foo timer is a number')
    process.emit('timeEnd', 'foo')
    process.emit('timeEnd', 'bar')
    process.emit('timeEnd', 'baz')
    // npm timer is started by default
    process.emit('timeEnd', 'npm')
    t.match(logs.timing, [
      ['foo', /Completed in [0-9]+ms/],
      ['bar', /Completed in [0-9]+ms/],
      ['npm', /Completed in [0-9]+ms/],
    ])
    t.match(logs.silly, [[
      'timing',
      "Tried to end timer that doesn't exist:",
      'baz',
    ]])
    t.notOk(npm.unfinishedTimers.has('foo'), 'foo timer is gone')
    t.notOk(npm.unfinishedTimers.has('bar'), 'bar timer is gone')
    t.match(npm.finishedTimers, { foo: Number, bar: Number, npm: Number })
    t.end()
  })

  t.test('writes timings file', async t => {
    const { npm, timingFile } = await loadMockNpm(t, {
      config: { timing: true },
    })
    process.emit('time', 'foo')
    process.emit('timeEnd', 'foo')
    process.emit('time', 'bar')
    npm.unload()
    const timings = await timingFile()
    t.match(timings, {
      command: [],
      logfile: String,
      logfiles: [String],
      version: String,
      unfinished: {
        bar: [Number, Number],
        npm: [Number, Number],
      },
      foo: Number,
      'npm:load': Number,
    })
  })

  t.test('does not write timings file with timers:false', async t => {
    const { npm, timingFile } = await loadMockNpm(t, {
      config: { false: true },
    })
    npm.unload()
    await t.rejects(() => timingFile())
  })
})

t.test('output clears progress and console.logs the message', async t => {
  t.plan(2)
  let showingProgress = true
  const logs = []
  mockGlobals(t, {
    'console.log': (...args) => {
      t.equal(showingProgress, false, 'should not be showing progress right now')
      logs.push(args)
    },
  })
  const { npm } = await loadMockNpm(t, {
    load: false,
    mocks: {
      npmlog: {
        clearProgress: () => showingProgress = false,
        showProgress: () => showingProgress = true,
      },
    },
  })
  npm.originalOutput('hello')
  t.match(logs, [['hello']])
  t.end()
})

t.test('unknown command', async t => {
  const { npm } = await loadMockNpm(t, { load: false })
  await t.rejects(
    npm.cmd('thisisnotacommand'),
    { code: 'EUNKNOWNCOMMAND' }
  )
})

t.test('explicit workspace rejection', async t => {
  mockGlobals(t, {
    'process.argv': [
      process.execPath,
      process.argv[1],
      '--color', 'false',
      '--workspace', './packages/a',
    ],
  })
  const mock = await loadMockNpm(t, {
    prefixDir: {
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
        workspaces: ['./packages/a'],
      }),
    },
  })
  await t.rejects(
    mock.npm.exec('ping', []),
    /This command does not support workspaces/
  )
})

t.test('implicit workspace rejection', async t => {
  const mock = await loadMockNpm(t, {
    prefixDir: {
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
        workspaces: ['./packages/a'],
      }),
    },
  })
  const cwd = join(mock.npm.config.localPrefix, 'packages', 'a')
  mock.npm.config.set('workspace', [cwd], 'default')
  mockGlobals(t, {
    'process.argv': [
      process.execPath,
      process.argv[1],
      '--color', 'false',
    ],
  })
  await t.rejects(
    mock.npm.exec('owner', []),
    /This command does not support workspaces/
  )
})

t.test('implicit workspace accept', async t => {
  const mock = await loadMockNpm(t, {
    prefixDir: {
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
        workspaces: ['./packages/a'],
      }),
    },
  })
  const cwd = join(mock.npm.config.localPrefix, 'packages', 'a')
  mock.npm.config.set('workspace', [cwd], 'default')
  mockGlobals(t, {
    'process.cwd': () => mock.npm.config.cwd,
    'process.argv': [
      process.execPath,
      process.argv[1],
      '--color', 'false',
    ],
  })
  await t.rejects(
    mock.npm.exec('org', []),
    /.*Usage/
  )
})
