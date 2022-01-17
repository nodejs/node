const t = require('tap')

const mockGlobals = require('../fixtures/mock-globals.js')
const { load: loadMockNpm } = require('../fixtures/mock-npm.js')

const cliMock = async (t, mocks) => {
  let exitHandlerArgs = null
  let npm = null
  const exitHandlerMock = (...args) => {
    exitHandlerArgs = args
    npm.unload()
  }
  exitHandlerMock.setNpm = _npm => npm = _npm

  const { Npm, outputs, logMocks, logs } = await loadMockNpm(t, { mocks, init: false })
  const cli = t.mock('../../lib/cli.js', {
    '../../lib/npm.js': Npm,
    '../../lib/utils/update-notifier.js': async () => null,
    '../../lib/utils/unsupported.js': {
      checkForBrokenNode: () => {},
      checkForUnsupportedNode: () => {},
    },
    '../../lib/utils/exit-handler.js': exitHandlerMock,
    ...logMocks,
  })

  return {
    Npm,
    cli,
    outputs,
    exitHandlerCalled: () => exitHandlerArgs,
    exitHandlerNpm: () => npm,
    logs,
  }
}

t.afterEach(() => {
  delete process.exitCode
})

t.test('print the version, and treat npm_g as npm -g', async t => {
  mockGlobals(t, {
    'process.argv': ['node', 'npm_g', '-v'],
  })

  const { logs, cli, Npm, outputs, exitHandlerCalled } = await cliMock(t)
  await cli(process)

  t.strictSame(process.argv, ['node', 'npm', '-g', '-v'], 'system process.argv was rewritten')
  t.strictSame(logs.verbose.filter(([p]) => p !== 'logfile'), [
    ['cli', process.argv],
  ])
  t.strictSame(logs.info, [
    ['using', 'npm@%s', Npm.version],
    ['using', 'node@%s', process.version],
  ])
  t.strictSame(outputs, [[Npm.version]])
  t.strictSame(exitHandlerCalled(), [])
})

t.test('calling with --versions calls npm version with no args', async t => {
  t.plan(6)
  mockGlobals(t, {
    'process.argv': ['node', 'npm', 'install', 'or', 'whatever', '--versions'],
  })
  const { logs, cli, Npm, outputs, exitHandlerCalled } = await cliMock(t, {
    '../../lib/commands/version.js': class Version {
      async exec (args) {
        t.strictSame(args, [])
      }
    },
  })

  await cli(process)
  t.equal(process.title, 'npm install or whatever')
  t.strictSame(logs.verbose.filter(([p]) => p !== 'logfile'), [
    ['cli', process.argv],
  ])
  t.strictSame(logs.info, [
    ['using', 'npm@%s', Npm.version],
    ['using', 'node@%s', process.version],
  ])

  t.strictSame(outputs, [])
  t.strictSame(exitHandlerCalled(), [])
})

t.test('logged argv is sanitized', async t => {
  mockGlobals(t, {
    'process.argv': [
      'node',
      'npm',
      'version',
      'https://username:password@npmjs.org/test_url_with_a_password',
    ],
  })
  const { logs, cli, Npm } = await cliMock(t, {
    '../../lib/commands/version.js': class Version {
      async exec (args) {}
    },
  })

  await cli(process)
  t.ok(process.title.startsWith('npm version https://username:***@npmjs.org'))
  t.strictSame(logs.verbose.filter(([p]) => p !== 'logfile'), [
    [
      'cli',
      ['node', 'npm', 'version', 'https://username:***@npmjs.org/test_url_with_a_password'],
    ],
  ])
  t.strictSame(logs.info, [
    ['using', 'npm@%s', Npm.version],
    ['using', 'node@%s', process.version],
  ])
})

t.test('print usage if no params provided', async t => {
  mockGlobals(t, {
    'process.argv': ['node', 'npm'],
  })

  const { cli, outputs, exitHandlerCalled, exitHandlerNpm } = await cliMock(t)
  await cli(process)
  t.match(outputs[0][0], 'Usage:', 'outputs npm usage')
  t.match(exitHandlerCalled(), [], 'should call exitHandler with no args')
  t.ok(exitHandlerNpm(), 'exitHandler npm is set')
  t.match(process.exitCode, 1)
})

t.test('print usage if non-command param provided', async t => {
  mockGlobals(t, {
    'process.argv': ['node', 'npm', 'tset'],
  })

  const { cli, outputs, exitHandlerCalled, exitHandlerNpm } = await cliMock(t)
  await cli(process)
  t.match(outputs[0][0], 'Unknown command: "tset"')
  t.match(outputs[0][0], 'Did you mean this?')
  t.match(exitHandlerCalled(), [], 'should call exitHandler with no args')
  t.ok(exitHandlerNpm(), 'exitHandler npm is set')
  t.match(process.exitCode, 1)
})

t.test('load error calls error handler', async t => {
  mockGlobals(t, {
    'process.argv': ['node', 'npm', 'asdf'],
  })

  const err = new Error('test load error')
  const { cli, exitHandlerCalled } = await cliMock(t, {
    '../../lib/utils/config/index.js': {
      definitions: null,
      flatten: null,
      shorthands: null,
    },
    '@npmcli/config': class BadConfig {
      async load () {
        throw err
      }
    },
  })
  await cli(process)
  t.strictSame(exitHandlerCalled(), [err])
})
