const t = require('tap')

let LOAD_ERROR = null
const npmock = {
  version: '99.99.99',
  load: cb => cb(LOAD_ERROR),
  argv: [],
  config: {
    settings: {},
    get: (k) => npmock.config.settings[k],
    set: (k, v) => {
      npmock.config.settings[k] = v
    },
  },
  commands: {},
}

const unsupportedMock = {
  checkForBrokenNode: () => {},
  checkForUnsupportedNode: () => {},
}

let errorHandlerCalled = null
const errorHandlerMock = (...args) => {
  errorHandlerCalled = args
}
let errorHandlerExitCalled = null
errorHandlerMock.exit = code => {
  errorHandlerExitCalled = code
}

const logs = []
const npmlogMock = {
  pause: () => logs.push('pause'),
  verbose: (...msg) => logs.push(['verbose', ...msg]),
  info: (...msg) => logs.push(['info', ...msg]),
}

const requireInject = require('require-inject')
const cli = requireInject.installGlobally('../../lib/cli.js', {
  '../../lib/npm.js': npmock,
  '../../lib/utils/unsupported.js': unsupportedMock,
  '../../lib/utils/error-handler.js': errorHandlerMock,
  npmlog: npmlogMock,
})

t.test('print the version, and treat npm_g to npm -g', t => {
  const { log } = console
  const consoleLogs = []
  console.log = (...msg) => consoleLogs.push(msg)
  const { argv } = process
  const proc = {
    argv: ['node', 'npm_g', '-v'],
    version: '420.69.lol',
    on: () => {},
  }
  process.argv = proc.argv
  npmock.config.settings.version = true

  cli(proc)

  t.strictSame(npmock.argv, [])
  t.strictSame(proc.argv, ['node', 'npm', '-g', '-v'])
  t.strictSame(logs, [
    'pause',
    ['verbose', 'cli', ['node', 'npm', '-g', '-v']],
    ['info', 'using', 'npm@%s', '99.99.99'],
    ['info', 'using', 'node@%s', '420.69.lol'],
  ])
  t.strictSame(consoleLogs, [['99.99.99']])
  t.strictSame(errorHandlerExitCalled, 0)

  delete npmock.config.settings.version
  process.argv = argv
  console.log = log
  npmock.argv.length = 0
  proc.argv.length = 0
  logs.length = 0
  consoleLogs.length = 0
  errorHandlerExitCalled = null

  t.end()
})

t.test('calling with --versions calls npm version with no args', t => {
  const { log } = console
  const consoleLogs = []
  console.log = (...msg) => consoleLogs.push(msg)
  const processArgv = process.argv
  const proc = {
    argv: ['node', 'npm', 'install', 'or', 'whatever', '--versions'],
    on: () => {},
  }
  process.argv = proc.argv
  npmock.config.set('versions', true)

  t.teardown(() => {
    delete npmock.config.settings.versions
    process.argv = processArgv
    console.log = log
    npmock.argv.length = 0
    proc.argv.length = 0
    logs.length = 0
    consoleLogs.length = 0
    errorHandlerExitCalled = null
    delete npmock.commands.version
  })

  npmock.commands.version = (args, cb) => {
    t.equal(proc.title, 'npm')
    t.strictSame(npmock.argv, [])
    t.strictSame(proc.argv, ['node', 'npm', 'install', 'or', 'whatever', '--versions'])
    t.strictSame(logs, [
      'pause',
      ['verbose', 'cli', ['node', 'npm', 'install', 'or', 'whatever', '--versions']],
      ['info', 'using', 'npm@%s', '99.99.99'],
      ['info', 'using', 'node@%s', undefined],
    ])

    t.strictSame(consoleLogs, [])
    t.strictSame(errorHandlerExitCalled, null)

    t.strictSame(args, [])
    t.end()
  }

  cli(proc)
})

t.test('print usage if -h provided', t => {
  const { log } = console
  const consoleLogs = []
  console.log = (...msg) => consoleLogs.push(msg)
  const proc = {
    argv: ['node', 'npm', 'asdf'],
    on: () => {},
  }
  npmock.argv = ['asdf']

  t.teardown(() => {
    console.log = log
    npmock.argv.length = 0
    proc.argv.length = 0
    logs.length = 0
    consoleLogs.length = 0
    errorHandlerExitCalled = null
    delete npmock.commands.help
  })

  npmock.commands.help = (args, cb) => {
    delete npmock.commands.help
    t.equal(proc.title, 'npm')
    t.strictSame(args, ['asdf'])
    t.strictSame(npmock.argv, ['asdf'])
    t.strictSame(proc.argv, ['node', 'npm', 'asdf'])
    t.strictSame(logs, [
      'pause',
      ['verbose', 'cli', ['node', 'npm', 'asdf']],
      ['info', 'using', 'npm@%s', '99.99.99'],
      ['info', 'using', 'node@%s', undefined],
    ])
    t.strictSame(consoleLogs, [])
    t.strictSame(errorHandlerExitCalled, null)
    t.end()
  }

  cli(proc)
})

t.test('load error calls error handler', t => {
  const er = new Error('poop')
  LOAD_ERROR = er
  const proc = {
    argv: ['node', 'npm', 'asdf'],
    on: () => {},
  }
  cli(proc)
  t.strictSame(errorHandlerCalled, [er])
  LOAD_ERROR = null
  t.end()
})
