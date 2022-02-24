const t = require('tap')
const os = require('os')
const EventEmitter = require('events')
const { format } = require('../../../lib/utils/log-file')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const mockGlobals = require('../../fixtures/mock-globals')
const { cleanCwd, cleanDate } = require('../../fixtures/clean-snapshot')

const pick = (obj, ...keys) => keys.reduce((acc, key) => {
  acc[key] = obj[key]
  return acc
}, {})

t.formatSnapshot = (obj) => {
  if (Array.isArray(obj)) {
    return obj
      .map((i) => Array.isArray(i) ? i.join(' ') : i)
      .join('\n')
  }
  return obj
}

t.cleanSnapshot = (path) => cleanDate(cleanCwd(path))
// Config loading is dependent on env so strip those from snapshots
  .replace(/.*timing config:load:.*\n/gm, '')
  .replace(/(Completed in )\d+(ms)/g, '$1{TIME}$2')

// cut off process from script so that it won't quit the test runner
// while trying to run through the myriad of cases.  need to make it
// have all the functions signal-exit relies on so that it doesn't
// nerf itself, thinking global.process is broken or gone.
mockGlobals(t, {
  process: Object.assign(new EventEmitter(), {
    ...pick(process, 'execPath', 'stdout', 'stderr', 'cwd', 'env'),
    argv: ['/node', ...process.argv.slice(1)],
    version: 'v1.0.0',
    kill: () => {},
    reallyExit: (code) => process.exit(code),
    pid: 123456,
    exit: (code) => {
      process.exitCode = code || process.exitCode || 0
      process.emit('exit', process.exitCode)
    },
  }),
}, { replace: true })

const mockExitHandler = async (t, { init, load, testdir, config } = {}) => {
  const errors = []
  mockGlobals(t, { 'console.error': (err) => errors.push(err) })

  const { npm, logMocks, ...rest } = await loadMockNpm(t, {
    init,
    load,
    testdir,
    mocks: {
      '../../package.json': {
        version: '1.0.0',
      },
    },
    config: {
      loglevel: 'notice',
      ...config,
    },
  })

  const exitHandler = t.mock('../../../lib/utils/exit-handler.js', {
    '../../../lib/utils/error-message.js': (err) => ({
      ...err,
      summary: [['ERR SUMMARY', err.message]],
      detail: [['ERR DETAIL', err.message]],
    }),
    os: {
      type: () => 'Foo',
      release: () => '1.0.0',
    },
    ...logMocks,
  })

  if (npm) {
    exitHandler.setNpm(npm)
  }

  t.teardown(() => {
    delete process.exitCode
    process.removeAllListeners('exit')
  })

  return {
    ...rest,
    errors,
    npm,
    // Make it async to make testing ergonomics a little
    // easier so we dont need to t.plan() every test to
    // make sure we get process.exit called
    exitHandler: (...args) => new Promise(resolve => {
      process.once('exit', resolve)
      exitHandler(...args)
    }),
  }
}

// Create errors with properties to be used in tests
const err = (message = '', options = {}, noStack = false) => {
  const e = Object.assign(
    new Error(message),
    typeof options !== 'object' ? { code: options } : options
  )
  e.stack = options.stack || `Error: ${message}`
  if (noStack) {
    delete e.stack
  }
  return e
}

t.test('handles unknown error with logs and debug file', async (t) => {
  const { exitHandler, debugFile, logs } = await mockExitHandler(t)

  await exitHandler(err('Unknown error', 'ECODE'))

  const debugContent = await debugFile()

  t.equal(process.exitCode, 1)
  logs.forEach((logItem, i) => {
    const logLines = format(i, ...logItem).trim().split(os.EOL)
    logLines.forEach((line) => {
      t.match(debugContent.trim(), line, 'log appears in debug file')
    })
  })

  const lastLog = debugContent
    .split('\n')
    .reduce((__, l) => parseInt(l.match(/^(\d+)\s/)[1]))
  t.equal(logs.length, lastLog + 1)
  t.match(logs.error, [
    ['code', 'ECODE'],
    ['ERR SUMMARY', 'Unknown error'],
    ['ERR DETAIL', 'Unknown error'],
  ])
  t.match(debugContent, /\d+ error code ECODE/)
  t.match(debugContent, /\d+ error ERR SUMMARY Unknown error/)
  t.match(debugContent, /\d+ error ERR DETAIL Unknown error/)
  t.matchSnapshot(logs, 'logs')
  t.matchSnapshot(debugContent, 'debug file contents')
})

t.test('exit handler never called - loglevel silent', async (t) => {
  const { logs, errors } = await mockExitHandler(t, {
    config: { loglevel: 'silent' },
  })
  process.emit('exit', 1)
  t.match(logs.error, [
    ['', /Exit handler never called/],
    ['', /error with npm itself/],
  ])
  t.strictSame(errors, [''], 'logs one empty string to console.error')
})

t.test('exit handler never called - loglevel notice', async (t) => {
  const { logs, errors } = await mockExitHandler(t)
  process.emit('exit', 1)
  t.equal(process.exitCode, 1)
  t.match(logs.error, [
    ['', /Exit handler never called/],
    ['', /error with npm itself/],
  ])
  t.strictSame(errors, ['', ''], 'logs two empty strings to console.error')
})

t.test('exit handler never called - no npm', async (t) => {
  const { logs, errors } = await mockExitHandler(t, { init: false })
  process.emit('exit', 1)
  t.equal(process.exitCode, 1)
  t.match(logs.error, [
    ['', /Exit handler never called/],
    ['', /error with npm itself/],
  ])
  t.strictSame(errors, [''], 'logs one empty string to console.error')
})

t.test('exit handler called - no npm', async (t) => {
  const { exitHandler, errors } = await mockExitHandler(t, { init: false })
  await exitHandler()
  t.equal(process.exitCode, 1)
  t.match(errors, [/Error: Exit prior to setting npm in exit handler/])
})

t.test('exit handler called - no npm with error', async (t) => {
  const { exitHandler, errors } = await mockExitHandler(t, { init: false })
  await exitHandler(err('something happened'))
  t.equal(process.exitCode, 1)
  t.match(errors, [/Error: something happened/])
})

t.test('exit handler called - no npm with error without stack', async (t) => {
  const { exitHandler, errors } = await mockExitHandler(t, { init: false })
  await exitHandler(err('something happened', {}, true))
  t.equal(process.exitCode, 1)
  t.match(errors, [/something happened/])
})

t.test('console.log output using --json', async (t) => {
  const { exitHandler, errors } = await mockExitHandler(t, {
    config: {
      json: true,
    },
  })

  await exitHandler(err('Error: EBADTHING Something happened'))

  t.equal(process.exitCode, 1)
  t.same(
    JSON.parse(errors[0]),
    {
      error: {
        code: 'EBADTHING', // should default error code to E[A-Z]+
        summary: 'Error: EBADTHING Something happened',
        detail: 'Error: EBADTHING Something happened',
      },
    },
    'should output expected json output'
  )
})

t.test('throw a non-error obj', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler({
    code: 'ESOMETHING',
    message: 'foo bar',
  })

  t.equal(process.exitCode, 1)
  t.match(logs.error, [
    ['weird error', { code: 'ESOMETHING', message: 'foo bar' }],
  ])
})

t.test('throw a string error', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler('foo bar')

  t.equal(process.exitCode, 1)
  t.match(logs.error, [
    ['', 'foo bar'],
  ])
})

t.test('update notification', async (t) => {
  const { exitHandler, logs, npm } = await mockExitHandler(t)
  npm.updateNotification = 'you should update npm!'

  await exitHandler()

  t.match(logs.notice, [
    ['', 'you should update npm!'],
  ])
})

t.test('npm.config not ready', async (t) => {
  const { exitHandler, logs, errors } = await mockExitHandler(t, {
    load: false,
  })

  await exitHandler()

  t.equal(process.exitCode, 1)
  t.match(errors, [
    /Error: Exit prior to config file resolving./,
  ], 'should exit with config error msg')
  t.match(logs.verbose, [
    ['stack', /Error: Exit prior to config file resolving./],
  ], 'should exit with config error msg')
})

t.test('timing with no error', async (t) => {
  const { exitHandler, timingFile, npm, logs } = await mockExitHandler(t, {
    config: {
      timing: true,
    },
  })

  await exitHandler()
  const timingFileData = await timingFile()

  t.equal(process.exitCode, 0)

  t.match(logs.error, [
    ['', /A complete log of this run can be found in:[\s\S]*-debug-\d\.log/],
  ])

  t.match(
    timingFileData,
    Object.keys(npm.finishedTimers).reduce((acc, k) => {
      acc[k] = Number
      return acc
    }, {})
  )
  t.strictSame(npm.unfinishedTimers, new Map())
  t.match(timingFileData, {
    command: [],
    version: '1.0.0',
    npm: Number,
    logfile: String,
    logfiles: [String],
  })
})

t.test('unfinished timers', async (t) => {
  const { exitHandler, timingFile, npm } = await mockExitHandler(t, {
    config: {
      timing: true,
    },
  })

  process.emit('time', 'foo')
  process.emit('time', 'bar')

  await exitHandler()
  const timingFileData = await timingFile()

  t.equal(process.exitCode, 0)
  t.match(npm.unfinishedTimers, new Map([['foo', Number], ['bar', Number]]))
  t.match(timingFileData, {
    command: [],
    version: '1.0.0',
    npm: Number,
    logfile: String,
    logfiles: [String],
    unfinished: {
      foo: [Number, Number],
      bar: [Number, Number],
    },
  })
})

t.test('uses code from errno', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler(err('Error with errno', { errno: 127 }))
  t.equal(process.exitCode, 127)
  t.match(logs.error, [['errno', 127]])
})

t.test('uses code from number', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler(err('Error with code type number', 404))
  t.equal(process.exitCode, 404)
  t.match(logs.error, [['code', 404]])
})

t.test('uses all err special properties', async t => {
  const { exitHandler, logs } = await mockExitHandler(t)

  const keys = ['code', 'syscall', 'file', 'path', 'dest', 'errno']
  const properties = keys.reduce((acc, k) => {
    acc[k] = `${k}-hey`
    return acc
  }, {})

  await exitHandler(err('Error with code type number', properties))
  t.equal(process.exitCode, 1)
  t.match(logs.error, keys.map((k) => [k, `${k}-hey`]), 'all special keys get logged')
})

t.test('verbose logs replace info on err props', async t => {
  const { exitHandler, logs } = await mockExitHandler(t)

  const keys = ['type', 'stack', 'statusCode', 'pkgid']
  const properties = keys.reduce((acc, k) => {
    acc[k] = `${k}-https://user:pass@registry.npmjs.org/`
    return acc
  }, {})

  await exitHandler(err('Error with code type number', properties))
  t.equal(process.exitCode, 1)
  t.match(
    logs.verbose.filter(([p]) => p !== 'logfile'),
    keys.map((k) => [k, `${k}-https://user:***@registry.npmjs.org/`]),
    'all special keys get replaced'
  )
})

t.test('call exitHandler with no error', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler()

  t.equal(process.exitCode, 0)
  t.match(logs.error, [])
})

t.test('defaults to log error msg if stack is missing when unloaded', async (t) => {
  const { exitHandler, logs, errors } = await mockExitHandler(t, { load: false })

  await exitHandler(err('Error with no stack', { code: 'ENOSTACK', errno: 127 }, true))
  t.equal(process.exitCode, 127)
  t.same(errors, ['Error with no stack'], 'should use error msg')
  t.match(logs.error, [
    ['code', 'ENOSTACK'],
    ['errno', 127],
  ])
})

t.test('exits uncleanly when only emitting exit event', async (t) => {
  const { logs } = await mockExitHandler(t)

  process.emit('exit')

  t.match(logs.error, [['', 'Exit handler never called!']])
  t.equal(process.exitCode, 1, 'exitCode coerced to 1')
  t.end()
})

t.test('do no fancy handling for shellouts', async t => {
  const { exitHandler, npm, logs } = await mockExitHandler(t)

  npm.command = 'exec'

  const loudNoises = () =>
    logs.filter(([level]) => ['warn', 'error'].includes(level))

  t.test('shellout with a numeric error code', async t => {
    await exitHandler(err('', 5))
    t.equal(process.exitCode, 5, 'got expected exit code')
    t.strictSame(loudNoises(), [], 'no noisy warnings')
  })

  t.test('shellout without a numeric error code (something in npm)', async t => {
    await exitHandler(err('', 'banana stand'))
    t.equal(process.exitCode, 1, 'got expected exit code')
    // should log some warnings and errors, because something weird happened
    t.strictNotSame(loudNoises(), [], 'bring the noise')
    t.end()
  })

  t.test('shellout with code=0 (extra weird?)', async t => {
    await exitHandler(Object.assign(new Error(), { code: 0 }))
    t.equal(process.exitCode, 1, 'got expected exit code')
    t.strictNotSame(loudNoises(), [], 'bring the noise')
  })

  t.end()
})
