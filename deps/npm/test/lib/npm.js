const t = require('tap')
const { resolve, dirname, join } = require('node:path')
const fs = require('node:fs/promises')
const { time } = require('proc-log')
const { load: loadMockNpm } = require('../fixtures/mock-npm.js')
const mockGlobals = require('@npmcli/mock-globals')
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
    npm.config.load = async () => {
      throw new Error('load error')
    }
    await t.rejects(
      () => npm.load(),
      /load error/
    )
  })

  await t.test('basic loading', async t => {
    const { npm, logs, cache } = await loadMockNpm(t, {
      prefixDir: { node_modules: {} },
      config: {
        timing: true,
      },
    })

    t.equal(npm.loaded, true)
    t.equal(npm.config.loaded, true)
    t.equal(npm.config.get('force'), false)
    t.ok(npm.usage, 'has usage')

    t.match(npm, {
      flatOptions: {},
    })

    t.match(logs.timing.filter((p) => /^npm:load/.test(p)), [
      /npm:load.* Completed in [0-9.]+ms/,
    ])

    mockGlobals(t, { process: { platform: 'posix' } })
    t.equal(resolve(npm.cache), resolve(cache), 'cache is cache')
    t.equal(npm.lockfileVersion, 2, 'lockfileVersion getter')
    t.equal(npm.prefix, npm.localPrefix, 'prefix is local prefix')
    t.not(npm.prefix, npm.globalPrefix, 'prefix is not global prefix')
    t.equal(npm.bin, npm.localBin, 'bin is local bin')
    t.not(npm.bin, npm.globalBin, 'bin is not global bin')

    npm.config.set('global', true)
    t.equal(npm.prefix, npm.globalPrefix, 'prefix is global prefix after setting global')
    t.not(npm.prefix, npm.localPrefix, 'prefix is not local prefix after setting global')
    t.equal(npm.bin, npm.globalBin, 'bin is global bin after setting global')
    t.not(npm.bin, npm.localBin, 'bin is not local bin after setting global')
    t.equal(npm.dir, npm.globalDir, 'dir is global dir after setting global')
    t.not(npm.dir, npm.localDir, 'dir is not local dir after setting global')

    mockGlobals(t, { process: { platform: 'win32' } })
    t.equal(npm.bin, npm.globalBin, 'bin is global bin in windows mode')
    t.equal(npm.dir, npm.globalDir, 'dir is global dir in windows mode')
  })

  await t.test('forceful loading', async t => {
    const { logs } = await loadMockNpm(t, {
      config: {
        force: true,
      },
    })
    t.match(logs.warn, [
      'using --force Recommended protections disabled.',
    ])
  })

  await t.test('node is a symlink', async t => {
    const node = process.platform === 'win32' ? 'node.exe' : 'node'
    const { Npm, npm, logs, outputs, prefix } = await loadMockNpm(t, {
      prefixDir: {
        bin: t.fixture('symlink', dirname(process.execPath)),
      },
      config: {
        timing: true,
        usage: '',
        scope: 'foo',
      },
      argv: [
        'token',
        'revoke',
        'blergggg',
      ],
      globals: (dirs) => ({
        'process.env.PATH': resolve(dirs.prefix, 'bin'),
        'process.argv': [
          node,
          process.argv[1],
        ],
      }),
    })

    t.equal(npm.config.get('scope'), '@foo', 'added the @ sign to scope')

    t.match([
      ...logs.timing.filter((p) => p.startsWith('npm:load:whichnode')),
      ...logs.verbose,
      ...logs.timing.filter((p) => p.startsWith('npm:load')),
    ], [
      /npm:load:whichnode Completed in [0-9.]+ms/,
      `node symlink ${resolve(prefix, 'bin', node)}`,
      /title npm token revoke blergggg/,
      /argv "token" "revoke" "blergggg".*"--usage" "--scope" "foo"/,
      /logfile logs-max:\d+ dir:.*/,
      /logfile .*-debug-0.log/,
      /npm:load:.* Completed in [0-9.]+ms/,
    ])
    t.equal(process.execPath, resolve(prefix, 'bin', node))

    outputs.length = 0
    logs.length = 0
    await npm.exec('ll', [])

    t.equal(npm.command, 'll', 'command set to first npm command')
    t.equal(npm.flatOptions.npmCommand, 'll', 'npmCommand flatOption set')

    const ll = Npm.cmd('ll')
    t.same(outputs, [ll.describeUsage], 'print usage')
    npm.config.set('usage', false)

    outputs.length = 0
    logs.length = 0
    await npm.exec('get', ['scope', 'usage'])

    t.strictSame([npm.command, npm.flatOptions.npmCommand], ['ll', 'll'],
      'does not change npm.command when another command is called')

    t.match(logs, [
      /timing config:load:flatten Completed in [0-9.]+ms/,
      /timing command:config Completed in [0-9.]+ms/,
    ])
    t.same(outputs, ['scope=@foo\nusage=false'])
  })

  await t.test('--no-workspaces with --workspace', async t => {
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
        },
        'package.json': JSON.stringify({
          name: 'root',
          version: '1.0.0',
          workspaces: ['./packages/*'],
        }),
      },
      config: {
        workspaces: false,
        workspace: 'a',
      },
    })
    await t.rejects(
      npm.exec('run', []),
      /Cannot use --no-workspaces and --workspace at the same time/
    )
  })

  await t.test('workspace-aware configs and commands', async t => {
    const { npm, joinedOutput } = await loadMockNpm(t, {
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
      config: {
        workspaces: true,
      },
    })

    await npm.exec('run-script', [])

    t.equal(npm.command, 'run', 'npm.command set to canonical name')

    t.matchSnapshot(joinedOutput(), 'should exec workspaces version of commands')
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
      config: {
        workspaces: true,
        global: true,
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
      config: {
        usage: true,
        scope: 'foo',
      },
      argv: ['ls'],
    })
    t.equal(npm.title, 'npm ls')
    t.equal(process.title, 'npm ls')
  })

  t.test('do not expose token being revoked', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        usage: true,
        scope: 'foo',
      },
      argv: ['token', 'revoke', `npm_${'a'.repeat(36)}`],
    })
    t.equal(npm.title, 'npm token revoke npm_***')
    t.equal(process.title, 'npm token revoke npm_***')
  })

  t.test('do show *** unless a token is actually being revoked', async t => {
    const { npm } = await loadMockNpm(t, {
      config: {
        usage: true,
        scope: 'foo',
      },
      argv: ['token', 'revoke', 'notatoken'],
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
    const log3 = ['silly', 'test', 'hello\x00world']

    process.emit('log', ...log1)
    await npm.load()
    process.emit('log', ...log2)
    process.emit('log', ...log3)

    const debug = await debugFile()
    t.equal(npm.logFiles.length, 1, 'one debug file')
    t.match(debug, log1.join(' '), 'before load appears')
    t.match(debug, log2.join(' '), 'after load log appears')
    t.match(debug, 'hello^@world')
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
    await fs.writeFile(logsDir, 'A_TEXT_FILE')
    await t.resolves(npm.load(), 'loads with invalid logs dir')

    t.equal(npm.logFiles.length, 0, 'no log files array')
    t.strictSame(await fs.readFile(logsDir, 'utf-8'), 'A_TEXT_FILE')
  })
})

t.test('cache dir', async t => {
  t.test('creates a cache dir', async t => {
    const { npm } = await loadMockNpm(t)

    await t.resolves(fs.access(npm.cache), 'cache dir exists')
  })

  t.test('can load with a bad cache dir', async t => {
    const { npm, cache } = await loadMockNpm(t, {
      load: false,
      // The easiest way to make mkdir(cache) fail is to make it a file.
      // This will have the same effect as if its read only or inaccessible.
      cacheDir: 'A_TEXT_FILE',
    })

    await t.resolves(npm.load(), 'loads with cache dir as a file')

    t.equal(await fs.readFile(cache, 'utf-8'), 'A_TEXT_FILE')
  })
})

t.test('timings', async t => {
  t.test('writes timings file', async t => {
    const { npm, timingFile } = await loadMockNpm(t, {
      config: { timing: true },
    })
    time.start('foo')
    time.end('foo')
    time.start('bar')
    npm.finish()
    const timings = await timingFile()
    t.match(timings, {
      metadata: {
        command: [],
        logfiles: [String],
        version: String,
      },
      unfinishedTimers: {
        bar: [Number, Number],
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
    npm.finish()
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
      const { logs } = await loadMockNpm(t, { config })
      t.equal(!!logs.length, expectedDisplay, 'display')
      t.equal(!!logs.timing.length, expectedTiming, 'timing display')
    })
  }
})

t.test('aliases and typos', async t => {
  const { Npm } = await loadMockNpm(t, { init: false })
  t.throws(() => Npm.cmd('thisisnotacommand'), { code: 'EUNKNOWNCOMMAND' })
  t.throws(() => Npm.cmd(''), { code: 'EUNKNOWNCOMMAND' })
  t.throws(() => Npm.cmd('birthday'), { code: 'EUNKNOWNCOMMAND' })
  t.match(Npm.cmd('it').name, 'install-test')
  t.match(Npm.cmd('installTe').name, 'install-test')
  t.match(Npm.cmd('access').name, 'access')
  t.match(Npm.cmd('auth').name, 'owner')
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
    config: {
      workspace: './packages/a',
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
    config: {
      workspace: './packages/a',
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
  })
  await t.rejects(mock.npm.exec('org', []), /.*Usage/)
})

t.test('subcommand handling', async t => {
  t.test('no subcommand provided', async t => {
    const { npm } = await loadMockNpm(t)
    await t.rejects(
      npm.exec('trust', []),
      /Usage/,
      'throws usage error when no subcommand provided'
    )
  })

  t.test('unknown subcommand', async t => {
    const { npm } = await loadMockNpm(t)
    await t.rejects(
      npm.exec('trust', ['unknown-subcommand']),
      /Unknown subcommand: unknown-subcommand/,
      'throws error for unknown subcommand'
    )
  })

  t.test('subcommand help with --usage', async t => {
    const { npm, outputs } = await loadMockNpm(t, {
      config: {
        usage: true,
      },
    })
    await npm.exec('trust', ['github'])
    t.ok(outputs.length > 0, 'outputs help text')
    // Check if output was generated - the format may be different
    t.ok(outputs.some(o => o && o[0]), 'has output content')
  })
})

t.test('exec edge cases', async t => {
  t.test('command calls exec again - covers else branch at line 207', async t => {
    const { npm, outputs } = await loadMockNpm(t)
    // 'get' command calls npm.exec('config', ['get', ...]) internally
    // The first exec() sets #command, then when it re-enters exec(),
    // the else branch (line 217) is taken because #command is already set
    await npm.exec('get', ['registry'])
    t.ok(outputs.length > 0, 'command executed and produced output')
  })

  t.test('exec without args parameter - covers default args branch', async t => {
    const Npm = require('../../lib/npm.js')
    const npm = new Npm()
    await npm.load()
    npm.argv = ['test']
    // Call exec without second parameter - should use default args = this.argv
    await npm.exec('run')
    t.pass('exec called without second argument')
  })

  t.test('--versions flag sets argv to version', async t => {
    const { npm } = await loadMockNpm(t, {
      config: { versions: true },
    })
    t.equal(npm.argv.length, 0, 'argv is empty after version command runs')
    t.equal(npm.config.get('usage'), false, 'usage is set to false')
  })

  t.test('color true sets COLOR env to 1', async t => {
    await loadMockNpm(t, {
      config: { color: 'always' },
    })
    t.equal(process.env.COLOR, '1', 'COLOR env is set to 1 when color is truthy')
  })

  t.test('command without subcommands', async t => {
    const { npm } = await loadMockNpm(t)
    // Test a command that doesn't have subcommands (line 249 branch)
    await t.rejects(npm.exec('org', []), /Usage/)
  })

  t.test('command with workspaces support', async t => {
    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        packages: {
          a: {
            'package.json': JSON.stringify({
              name: 'a',
              version: '1.0.0',
              scripts: { test: 'echo test' },
            }),
          },
        },
        'package.json': JSON.stringify({
          name: 'root',
          version: '1.0.0',
          workspaces: ['./packages/a'],
        }),
      },
      config: {
        workspace: ['./packages/a'],
      },
    })
    // Test a command that supports workspaces to trigger execWorkspaces path (line 321)
    await npm.exec('run', ['test'])
    t.pass('executes with workspaces')
  })

  t.test('execCommandClass with default commandPath', async t => {
    const { npm } = await loadMockNpm(t)
    // Create a simple command instance
    const Command = npm.constructor.cmd('version')
    const commandInstance = new Command(npm)

    // Call execCommandClass without providing commandPath (using default [])
    await npm.execCommandClass(commandInstance, [])

    t.pass('execCommandClass works with default commandPath parameter')
  })

  t.test('command with definitions executes exec() without workspaces', async t => {
    const BaseCommand = require('../../lib/base-cmd.js')
    const Definition = require('@npmcli/config/lib/definitions/definition.js')

    let execCalled = false
    let execArgs = null
    let execFlags = null

    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'test-pkg',
          version: '1.0.0',
        }),
      },
    })

    class TestCommand extends BaseCommand {
      static name = 'test-cmd'
      static description = 'Test command with definitions'
      static workspaces = true
      static definitions = [
        new Definition('testflag', {
          type: String,
          default: 'default-value',
          description: 'A test flag',
        }),
      ]

      async exec (args, flags) {
        execCalled = true
        execArgs = args
        execFlags = flags
      }

      async execWorkspaces () {
        throw new Error('execWorkspaces should not be called')
      }
    }

    const command = new TestCommand(npm)
    // Set config.argv so flags() can parse the positional args
    npm.config.argv = [process.argv[0], process.argv[1], 'test-cmd', 'arg1', 'arg2']
    await npm.execCommandClass(command, ['arg1', 'arg2'], ['test-cmd'])

    t.equal(execCalled, true, 'exec() was called')
    t.same(execArgs, ['arg1', 'arg2'], 'positional args passed correctly')
    t.ok(execFlags, 'flags object passed')
    t.equal(execFlags.testflag, 'default-value', 'flag has default value')
  })

  t.test('command with definitions executes execWorkspaces() with workspaces', async t => {
    const BaseCommand = require('../../lib/base-cmd.js')
    const Definition = require('@npmcli/config/lib/definitions/definition.js')

    let execWorkspacesCalled = false
    let execArgs = null
    let execFlags = null

    const { npm } = await loadMockNpm(t, {
      prefixDir: {
        packages: {
          a: {
            'package.json': JSON.stringify({
              name: 'workspace-a',
              version: '1.0.0',
            }),
          },
        },
        'package.json': JSON.stringify({
          name: 'root',
          version: '1.0.0',
          workspaces: ['./packages/a'],
        }),
      },
      config: {
        workspace: ['./packages/a'],
      },
    })

    class TestCommand extends BaseCommand {
      static name = 'test-cmd'
      static description = 'Test command with definitions'
      static workspaces = true
      static definitions = [
        new Definition('testflag', {
          type: String,
          default: 'ws-default',
          description: 'A test flag',
        }),
      ]

      async exec () {
        throw new Error('exec should not be called')
      }

      async execWorkspaces (args, flags) {
        execWorkspacesCalled = true
        execArgs = args
        execFlags = flags
      }
    }

    const command = new TestCommand(npm)
    // Set config.argv so flags() can parse the positional args
    npm.config.argv = [process.argv[0], process.argv[1], 'test-cmd', 'wsarg1']
    await npm.execCommandClass(command, ['wsarg1'], ['test-cmd'])

    t.equal(execWorkspacesCalled, true, 'execWorkspaces() was called')
    t.same(execArgs, ['wsarg1'], 'positional args passed correctly')
    t.ok(execFlags, 'flags object passed')
    t.equal(execFlags.testflag, 'ws-default', 'flag has default value')
  })
})

t.test('usage', async t => {
  t.test('with browser', async t => {
    const { npm } = await loadMockNpm(t, { globals: { process: { platform: 'posix' } } })
    const usage = npm.usage
    npm.config.set('viewer', 'browser')
    const browserUsage = npm.usage
    t.notMatch(usage, '(in a browser)')
    t.match(browserUsage, '(in a browser)')
  })

  t.test('windows always uses browser', async t => {
    const { npm } = await loadMockNpm(t, { globals: { process: { platform: 'win32' } } })
    const usage = npm.usage
    npm.config.set('viewer', 'browser')
    const browserUsage = npm.usage
    t.match(usage, '(in a browser)')
    t.match(browserUsage, '(in a browser)')
  })

  t.test('includes commands', async t => {
    const { npm } = await loadMockNpm(t)
    const usage = npm.usage
    npm.config.set('long', true)
    const longUsage = npm.usage

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
        const usage = npm.usage
        t.matchSnapshot(usage)
      })
    }
  })
})
