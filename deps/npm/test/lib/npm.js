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
    } else
      t.match(process.env[env], /^(run-script|exec)$/)
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
  for (const env of Object.keys(process.env).filter(e => /^npm_/.test(e)))
    delete process.env[env]
  process.env.npm_config_cache = CACHE
  process.argv = argv
  Object.defineProperty(process, 'platform', {
    value: actualPlatform,
    configurable: true,
  })
})

const CACHE = t.testdir()
process.env.npm_config_cache = CACHE

t.test('not yet loaded', t => {
  const { npm, logs } = mockNpm(t)
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
  const list = npm.commands.list
  t.throws(() => npm.commands.list())
  t.equal(npm.commands.ls, list)
  t.equal(npm.commands.list, list)
  t.equal(npm.commands.asdfasdf, undefined)
  t.equal(npm.deref('list'), 'ls')
  t.same(logs, [])
  t.end()
})

t.test('npm.load', t => {
  t.test('callback must be a function', t => {
    const { npm, logs } = mockNpm(t)
    const er = new TypeError('callback must be a function if provided')
    t.throws(() => npm.load({}), er)
    t.same(logs, [])
    t.end()
  })

  t.test('callback style', t => {
    const { npm } = mockNpm(t)
    npm.load((err) => {
      if (err)
        throw err
      t.ok(npm.loaded)
      t.end()
    })
  })

  t.test('load error', async t => {
    const { npm } = mockNpm(t)
    const loadError = new Error('load error')
    npm.config.load = async () => {
      throw loadError
    }
    await npm.load().catch(er => {
      t.equal(er, loadError)
      t.equal(npm.loadErr, loadError)
    })
    npm.config.load = async () => {
      throw new Error('new load error')
    }
    await npm.load().catch(er => {
      t.equal(er, loadError, 'loading again returns the original error')
      t.equal(npm.loadErr, loadError)
    })
  })

  t.test('basic loading', async t => {
    const { npm, logs } = mockNpm(t)
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
    const { npm, logs } = mockNpm(t)
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

    const { npm, logs, outputs } = mockNpm(t)
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
    await npm.commands.ll([], (er) => {
      if (er)
        throw er

      t.equal(npm.command, 'll', 'command set to first npm command')
      t.equal(npm.flatOptions.npmCommand, 'll', 'npmCommand flatOption set')

      t.same(outputs, [[npm.commands.ll.usage]], 'print usage')
      npm.config.set('usage', false)
      t.equal(npm.commands.ll, npm.commands.ll, 'same command, different name')
    })

    outputs.length = 0
    logs.length = 0
    await npm.commands.get(['scope', '\u2010not-a-dash'], (er) => {
      if (er)
        throw er

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

    // need this here or node 10 will improperly end the promise ahead of time
    await new Promise((res) => setTimeout(res))
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

    const { npm, outputs } = mockNpm(t)
    await npm.load()
    npm.localPrefix = dir

    await new Promise((res, rej) => {
      // verify that calling the command with a short name still sets
      // the npm.command property to the full canonical name of the cmd.
      npm.command = null
      npm.commands.run([], er => {
        if (er)
          rej(er)

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

        res()
      })
    })
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
    const { npm } = mockNpm(t)
    await npm.load()
    npm.localPrefix = dir
    await new Promise((res, rej) => {
      // verify that calling the command with a short name still sets
      // the npm.command property to the full canonical name of the cmd.
      npm.command = null
      npm.commands.run([], er => {
        t.match(er, /Workspaces not supported for global packages/)
        res()
      })
    })
  })
  t.end()
})

t.test('loading as main will load the cli', t => {
  const { spawn } = require('child_process')
  const npm = require.resolve('../../lib/npm.js')
  const LS = require('../../lib/ls.js')
  const ls = new LS({})
  const p = spawn(process.execPath, [npm, 'ls', '-h'])
  const out = []
  p.stdout.on('data', c => out.push(c))
  p.on('close', (code, signal) => {
    t.equal(code, 0)
    t.equal(signal, null)
    t.match(Buffer.concat(out).toString(), ls.usage)
    t.end()
  })
})

t.test('set process.title', t => {
  t.test('basic title setting', async t => {
    process.argv = [
      process.execPath,
      process.argv[1],
      '--usage',
      '--scope=foo',
      'ls',
    ]
    const { npm } = mockNpm(t)
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
    const { npm } = mockNpm(t)
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
    const { npm } = mockNpm(t)
    await npm.load()
    t.equal(npm.title, 'npm token revoke')
    t.equal(process.title, 'npm token revoke')
  })

  t.end()
})
