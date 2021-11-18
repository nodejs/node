const t = require('tap')

const { real: mockNpm } = require('../fixtures/mock-npm.js')

const unsupportedMock = {
  checkForBrokenNode: () => {},
  checkForUnsupportedNode: () => {},
}

let exitHandlerCalled = null
let exitHandlerNpm = null
let exitHandlerCb
const exitHandlerMock = (...args) => {
  exitHandlerCalled = args
  if (exitHandlerCb) {
    exitHandlerCb()
  }
}
exitHandlerMock.setNpm = npm => {
  exitHandlerNpm = npm
}

const logs = []
const npmlogMock = {
  pause: () => logs.push('pause'),
  verbose: (...msg) => logs.push(['verbose', ...msg]),
  info: (...msg) => logs.push(['info', ...msg]),
}

const cliMock = Npm =>
  t.mock('../../lib/cli.js', {
    '../../lib/npm.js': Npm,
    '../../lib/utils/update-notifier.js': async () => null,
    '../../lib/utils/unsupported.js': unsupportedMock,
    '../../lib/utils/exit-handler.js': exitHandlerMock,
    npmlog: npmlogMock,
  })

const processMock = proc => {
  const mocked = {
    ...process,
    on: () => {},
    ...proc,
  }
  // nopt looks at process directly
  process.argv = mocked.argv
  return mocked
}

const { argv } = process

t.afterEach(() => {
  logs.length = 0
  process.argv = argv
  exitHandlerCalled = null
  exitHandlerNpm = null
})

t.test('print the version, and treat npm_g as npm -g', async t => {
  const proc = processMock({
    argv: ['node', 'npm_g', '-v'],
    version: process.version,
  })

  const { Npm, outputs } = mockNpm(t)
  const cli = cliMock(Npm)
  await cli(proc)

  t.strictSame(proc.argv, ['node', 'npm', '-g', '-v'], 'npm process.argv was rewritten')
  t.strictSame(process.argv, ['node', 'npm', '-g', '-v'], 'system process.argv was rewritten')
  t.strictSame(logs, [
    'pause',
    ['verbose', 'cli', proc.argv],
    ['info', 'using', 'npm@%s', Npm.version],
    ['info', 'using', 'node@%s', process.version],
  ])
  t.strictSame(outputs, [[Npm.version]])
  t.strictSame(exitHandlerCalled, [])
})

t.test('calling with --versions calls npm version with no args', async t => {
  t.plan(5)
  const proc = processMock({
    argv: ['node', 'npm', 'install', 'or', 'whatever', '--versions'],
  })
  const { Npm, outputs } = mockNpm(t, {
    '../../lib/commands/version.js': class Version {
      async exec (args) {
        t.strictSame(args, [])
      }
    },
  })
  const cli = cliMock(Npm)
  await cli(proc)
  t.equal(proc.title, 'npm')
  t.strictSame(logs, [
    'pause',
    ['verbose', 'cli', proc.argv],
    ['info', 'using', 'npm@%s', Npm.version],
    ['info', 'using', 'node@%s', process.version],
  ])

  t.strictSame(outputs, [])
  t.strictSame(exitHandlerCalled, [])
})

t.test('logged argv is sanitized', async t => {
  const proc = processMock({
    argv: [
      'node',
      'npm',
      'version',
      'https://username:password@npmjs.org/test_url_with_a_password',
    ],
  })
  const { Npm } = mockNpm(t, {
    '../../lib/commands/version.js': class Version {
      async exec (args) {}
    },
  })

  const cli = cliMock(Npm)

  await cli(proc)
  t.equal(proc.title, 'npm')
  t.strictSame(logs, [
    'pause',
    [
      'verbose',
      'cli',
      ['node', 'npm', 'version', 'https://username:***@npmjs.org/test_url_with_a_password'],
    ],
    ['info', 'using', 'npm@%s', Npm.version],
    ['info', 'using', 'node@%s', process.version],
  ])
})

t.test('print usage if no params provided', async t => {
  const proc = processMock({
    argv: ['node', 'npm'],
  })

  const { Npm, outputs } = mockNpm(t)
  const cli = cliMock(Npm)
  await cli(proc)
  t.match(outputs[0][0], 'Usage:', 'outputs npm usage')
  t.match(exitHandlerCalled, [], 'should call exitHandler with no args')
  t.ok(exitHandlerNpm, 'exitHandler npm is set')
  t.match(proc.exitCode, 1)
})

t.test('print usage if non-command param provided', async t => {
  const proc = processMock({
    argv: ['node', 'npm', 'tset'],
  })

  const { Npm, outputs } = mockNpm(t)
  const cli = cliMock(Npm)
  await cli(proc)
  t.match(outputs[0][0], 'Unknown command: "tset"')
  t.match(outputs[0][0], 'Did you mean this?')
  t.match(exitHandlerCalled, [], 'should call exitHandler with no args')
  t.ok(exitHandlerNpm, 'exitHandler npm is set')
  t.match(proc.exitCode, 1)
})

t.test('load error calls error handler', async t => {
  const proc = processMock({
    argv: ['node', 'npm', 'asdf'],
  })

  const err = new Error('test load error')
  const { Npm } = mockNpm(t, {
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
  const cli = cliMock(Npm)
  await cli(proc)
  t.strictSame(exitHandlerCalled, [err])
})
