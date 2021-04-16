const t = require('tap')

let LOAD_ERROR = null
const npmOutputs = []
const npmock = {
  log: { level: 'silent' },
  output: (...msg) => npmOutputs.push(msg),
  usage: 'npm usage test example',
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
let errorHandlerCb
const errorHandlerMock = (...args) => {
  errorHandlerCalled = args
  if (errorHandlerCb)
    errorHandlerCb()
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

const cli = t.mock('../../lib/cli.js', {
  '../../lib/npm.js': npmock,
  '../../lib/utils/did-you-mean.js': () => '\ntest did you mean',
  '../../lib/utils/unsupported.js': unsupportedMock,
  '../../lib/utils/error-handler.js': errorHandlerMock,
  npmlog: npmlogMock,
})

t.test('print the version, and treat npm_g to npm -g', t => {
  t.teardown(() => {
    delete npmock.config.settings.version
    process.argv = argv
    npmock.argv.length = 0
    proc.argv.length = 0
    logs.length = 0
    npmOutputs.length = 0
    errorHandlerExitCalled = null
  })

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
  t.strictSame(npmOutputs, [['99.99.99']])
  t.strictSame(errorHandlerExitCalled, 0)

  t.end()
})

t.test('calling with --versions calls npm version with no args', t => {
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
    npmock.argv.length = 0
    proc.argv.length = 0
    logs.length = 0
    npmOutputs.length = 0
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

    t.strictSame(npmOutputs, [])
    t.strictSame(errorHandlerExitCalled, null)

    t.strictSame(args, [])
    t.end()
  }

  cli(proc)
})

t.test('print usage if no params provided', t => {
  const { output } = npmock
  t.teardown(() => {
    npmock.output = output
  })
  const proc = {
    argv: ['node', 'npm'],
    on: () => {},
  }
  npmock.argv = []
  npmock.output = (msg) => {
    if (msg) {
      t.match(msg, 'npm usage test example', 'outputs npm usage')
      t.end()
    }
  }
  cli(proc)
})

t.test('print usage if non-command param provided', t => {
  const { output } = npmock
  t.teardown(() => {
    npmock.output = output
  })
  const proc = {
    argv: ['node', 'npm', 'asdf'],
    on: () => {},
  }
  npmock.argv = ['asdf']
  npmock.output = (msg) => {
    if (msg) {
      t.match(msg, 'Unknown command: "asdf"\ntest did you mean', 'outputs did you mean')
      t.end()
    }
  }
  cli(proc)
})

t.test('gracefully handles error printing usage', t => {
  const { output } = npmock
  t.teardown(() => {
    npmock.output = output
    errorHandlerCb = null
    errorHandlerCalled = null
  })
  const proc = {
    argv: ['node', 'npm'],
    on: () => {},
  }
  npmock.argv = []
  errorHandlerCb = () => {
    t.match(errorHandlerCalled, [], 'should call errorHandler with no args')
    t.end()
  }
  cli(proc)
})

t.test('handles output error', t => {
  const { output } = npmock
  t.teardown(() => {
    npmock.output = output
    errorHandlerCb = null
    errorHandlerCalled = null
  })
  const proc = {
    argv: ['node', 'npm'],
    on: () => {},
  }
  npmock.argv = []
  npmock.output = () => {
    throw new Error('ERR')
  }
  errorHandlerCb = () => {
    t.match(errorHandlerCalled, /ERR/, 'should call errorHandler with error')
    t.end()
  }
  cli(proc)
})

t.test('load error calls error handler', t => {
  t.teardown(() => {
    errorHandlerCb = null
    errorHandlerCalled = null
    LOAD_ERROR = null
  })

  const er = new Error('test load error')
  LOAD_ERROR = er
  const proc = {
    argv: ['node', 'npm', 'asdf'],
    on: () => {},
  }
  errorHandlerCb = () => {
    t.strictSame(errorHandlerCalled, [er])
    t.end()
  }
  cli(proc)
})
