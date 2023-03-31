const t = require('tap')
const { resolve, dirname, join } = require('path')
const fs = require('fs')
const { load: loadMockNpm } = require('../fixtures/mock-npm.js')
const mockGlobals = require('../fixtures/mock-globals')
const { commands } = require('../../lib/utils/cmd-list.js')

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
  })
  t.throws(() => npm.config.set('foo', 'bar'))
  t.throws(() => npm.config.get('foo'))
  t.same(logs, [])
})

t.test('npm.load', async t => {
  await t.test('load error', async t => {
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

  await t.test('basic loading', async t => {
    const { npm, logs, prefix: dir, cache, other } = await loadMockNpm(t, {
      prefixDir: { node_modules: {} },
      otherDirs: {
        newCache: {},
      },
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
    npm.cache = other.newCache
    t.equal(npm.config.get('cache'), other.newCache, 'cache setter sets config')
    t.equal(npm.cache, other.newCache, 'cache getter gets new config')
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

  await t.test('forceful loading', async t => {
    const { logs } = await loadMockNpm(t, {
      globals: {
        'process.argv': [...process.argv, '--force', '--color', 'always'],
      },
    })
    t.match(logs.warn, [
      [
        'using --force',
        'Recommended protections disabled.',
      ],
    ])
  })

  await t.test('node is a symlink', async t => {
    const node = process.platform === 'win32' ? 'node.exe' : 'node'
    const { npm, logs, outputs, prefix } = await loadMockNpm(t, {
      prefixDir: {
        bin: t.fixture('symlink', dirname(process.execPath)),
      },
      globals: (dirs) => ({
        'process.env.PATH': resolve(dirs.prefix, 'bin'),
        'process.argv': [
          node,
          process.argv[1],
          '--usage',
          '--scope=foo',
          'token',
          'revoke',
          'blergggg',
        ],
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
      ['title', 'npm token revoke blergggg'],
      ['argv', '"--usage" "--scope" "foo" "token" "revoke" "blergggg"'],
      ['logfile', /logs-max:\d+ dir:.*/],
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

  await t.test('--no-workspaces with --workspace', async t => {
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
      globals: {
        'process.argv': [
          process.execPath,
          process.argv[1],
          '--color', 'false',
          '--workspaces', 'false',
          '--workspace', 'a',
        ],
      },
    })
    await t.rejects(
      npm.exec('run', []),
      /Can not use --no-workspaces and --workspace at the same time/
    )
  })

  await t.test('workspace-aware configs and commands', async t => {
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
      globals: {
        'process.argv': [
          process.execPath,
          process.argv[1],
          '--color', 'false',
          '--workspaces', 'true',
        ],
      },
    })

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

  await t.test('workspaces in global mode', async t => {
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
      globals: {
        'process.argv': [
          process.execPath,
          process.argv[1],
          '--color',
          'false',
          '--workspaces',
          '--global',
          'true',
        ],
      },
    })

    await t.rejects(
      npm.exec('run', []),
      /Workspaces not supported for global packages/
    )
  })
})

t.test('set process.title', async t => {
  t.test('basic title setting', async t => {
    const { npm } = await loadMockNpm(t, {
      globals: {
        'process.argv': [
          process.execPath,
          process.argv[1],
          '--usage',
          '--scope=foo',
          'ls',
        ],
      },
    })
    t.equal(npm.title, 'npm ls')
    t.equal(process.title, 'npm ls')
  })

  t.test('do not expose token being revoked', async t => {
    const { npm } = await loadMockNpm(t, {
      globals: {
        'process.argv': [
          process.execPath,
          process.argv[1],
          '--usage',
          '--scope=foo',
          'token',
          'revoke',
          `npm_${'a'.repeat(36)}`,
        ],
      },
    })
    t.equal(npm.title, 'npm token revoke npm_***')
    t.equal(process.title, 'npm token revoke npm_***')
  })

  t.test('do show *** unless a token is actually being revoked', async t => {
    const { npm } = await loadMockNpm(t, {
      globals: {
        'process.argv': [
          process.execPath,
          process.argv[1],
          '--usage',
          '--scope=foo',
          'token',
          'revoke',
          'notatoken',
        ],
      },
    })
    t.equal(npm.title, 'npm token revoke notatoken')
    t.equal(process.title, 'npm token revoke notatoken')
  })
})

t.test('debug log', async t => {
  t.test('writes log file', async t => {
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

  t.test('can load with bad dir', async t => {
    const { npm, testdir } = await loadMockNpm(t, {
      load: false,
      config: (dirs) => ({
        'logs-dir': join(dirs.testdir, 'my_logs_dir'),
      }),
    })
    const logsDir = join(testdir, 'my_logs_dir')

    // make logs dir a file before load so it files
    fs.writeFileSync(logsDir, 'A_TEXT_FILE')
    await t.resolves(npm.load(), 'loads with invalid logs dir')

    t.equal(npm.logFiles.length, 0, 'no log files array')
    t.strictSame(fs.readFileSync(logsDir, 'utf-8'), 'A_TEXT_FILE')
  })
})

t.test('cache dir', async t => {
  t.test('creates a cache dir', async t => {
    const { npm } = await loadMockNpm(t)

    t.ok(fs.existsSync(npm.cache), 'cache dir exists')
  })

  t.test('can load with a bad cache dir', async t => {
    const { npm, cache } = await loadMockNpm(t, {
      load: false,
      // The easiest way to make mkdir(cache) fail is to make it a file.
      // This will have the same effect as if its read only or inaccessible.
      cacheDir: 'A_TEXT_FILE',
    })

    await t.resolves(npm.load(), 'loads with cache dir as a file')

    t.equal(fs.readFileSync(cache, 'utf-8'), 'A_TEXT_FILE')
  })
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
  })

  t.test('writes timings file', async t => {
    const { npm, cache, timingFile } = await loadMockNpm(t, {
      config: { timing: true },
    })
    process.emit('time', 'foo')
    process.emit('timeEnd', 'foo')
    process.emit('time', 'bar')
    npm.writeTimingFile()
    t.match(npm.timingFile, cache)
    t.match(npm.timingFile, /-timing.json$/)
    const timings = await timingFile()
    t.match(timings, {
      metadata: {
        command: [],
        logfiles: [String],
        version: String,
      },
      unfinishedTimers: {
        bar: [Number, Number],
        npm: [Number, Number],
      },
      timers: {
        foo: Number,
        'npm:load': Number,
      },
    })
  })

  t.test('does not write timings file with timers:false', async t => {
    const { npm, timingFile } = await loadMockNpm(t, {
      config: { timing: false },
    })
    npm.writeTimingFile()
    await t.rejects(() => timingFile())
  })

  const timingDisplay = [
    [{ loglevel: 'silly' }, true, false],
    [{ loglevel: 'silly', timing: true }, true, true],
    [{ loglevel: 'silent', timing: true }, false, false],
  ]

  for (const [config, expectedDisplay, expectedTiming] of timingDisplay) {
    const msg = `${JSON.stringify(config)}, display:${expectedDisplay}, timing:${expectedTiming}`
    await t.test(`timing display: ${msg}`, async t => {
      const { display } = await loadMockNpm(t, { config })
      t.equal(!!display.length, expectedDisplay, 'display')
      t.equal(!!display.timing.length, expectedTiming, 'timing display')
    })
  }
})

t.test('output clears progress and console.logs the message', async t => {
  t.plan(4)
  let showingProgress = true
  const logs = []
  const errors = []
  const { npm } = await loadMockNpm(t, {
    load: false,
    mocks: {
      npmlog: {
        clearProgress: () => showingProgress = false,
        showProgress: () => showingProgress = true,
      },
    },
    globals: {
      'console.log': (...args) => {
        t.equal(showingProgress, false, 'should not be showing progress right now')
        logs.push(args)
      },
      'console.error': (...args) => {
        t.equal(showingProgress, false, 'should not be showing progress right now')
        errors.push(args)
      },
    },
  })
  npm.originalOutput('hello')
  npm.originalOutputError('error')

  t.match(logs, [['hello']])
  t.match(errors, [['error']])
})

t.test('aliases and typos', async t => {
  const { npm } = await loadMockNpm(t, { load: false })
  await t.rejects(npm.cmd('thisisnotacommand'), { code: 'EUNKNOWNCOMMAND' })
  await t.rejects(npm.cmd(''), { code: 'EUNKNOWNCOMMAND' })
  await t.rejects(npm.cmd('birthday'), { code: 'EUNKNOWNCOMMAND' })
  await t.resolves(npm.cmd('it'), { name: 'install-test' })
  await t.resolves(npm.cmd('installTe'), { name: 'install-test' })
  await t.resolves(npm.cmd('access'), { name: 'access' })
  await t.resolves(npm.cmd('auth'), { name: 'owner' })
})

t.test('explicit workspace rejection', async t => {
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
    globals: {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--color', 'false',
        '--workspace', './packages/a',
      ],
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
    chdir: ({ prefix }) => join(prefix, 'packages', 'a'),
    globals: {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--color', 'false',
        '--workspace', './packages/a',
      ],
    },
  })
  await t.rejects(
    mock.npm.exec('team', []),
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
    chdir: ({ prefix }) => join(prefix, 'packages', 'a'),
    globals: {
      'process.argv': [
        process.execPath,
        process.argv[1],
        '--color', 'false',
      ],
    },
  })
  await t.rejects(mock.npm.exec('org', []), /.*Usage/)
})

t.test('usage', async t => {
  t.test('with browser', async t => {
    mockGlobals(t, { process: { platform: 'posix' } })
    const { npm } = await loadMockNpm(t)
    const usage = await npm.usage
    npm.config.set('viewer', 'browser')
    const browserUsage = await npm.usage
    t.notMatch(usage, '(in a browser)')
    t.match(browserUsage, '(in a browser)')
  })

  t.test('windows always uses browser', async t => {
    mockGlobals(t, { process: { platform: 'win32' } })
    const { npm } = await loadMockNpm(t)
    const usage = await npm.usage
    npm.config.set('viewer', 'browser')
    const browserUsage = await npm.usage
    t.match(usage, '(in a browser)')
    t.match(browserUsage, '(in a browser)')
  })

  t.test('includes commands', async t => {
    const { npm } = await loadMockNpm(t)
    const usage = await npm.usage
    npm.config.set('long', true)
    const longUsage = await npm.usage

    const lastCmd = commands[commands.length - 1]
    for (const cmd of commands) {
      const isLast = cmd === lastCmd
      const shortCmd = new RegExp(`\\s${cmd}${isLast ? '\\n' : ',[\\s\\n]'}`)
      const longCmd = new RegExp(`^\\s+${cmd}\\s+\\w.*\n\\s+Usage:\\n`, 'm')

      t.match(usage, shortCmd, `usage includes ${cmd}`)
      t.notMatch(usage, longCmd, `usage does not include long ${cmd}`)

      t.match(longUsage, longCmd, `long usage includes ${cmd}`)
      if (!isLast) {
        // long usage includes false positives for the last command since it is
        // not followed by a comma
        t.notMatch(longUsage, shortCmd, `long usage does not include short ${cmd}`)
      }
    }
  })

  t.test('set process.stdout.columns', async t => {
    const { npm } = await loadMockNpm(t, {
      config: { viewer: 'man' },
    })
    t.cleanSnapshot = str =>
      str.replace(npm.config.get('userconfig'), '{USERCONFIG}')
        .replace(npm.npmRoot, '{NPMROOT}')
        .replace(`npm@${npm.version}`, 'npm@{VERSION}')

    const widths = [0, 1, 10, 24, 40, 41, 75, 76, 90, 100]
    for (const width of widths) {
      t.test(`column width ${width}`, async t => {
        mockGlobals(t, { 'process.stdout.columns': width })
        const usage = await npm.usage
        t.matchSnapshot(usage)
      })
    }
  })
})
