/* eslint-disable no-extend-native */
/* eslint-disable no-global-assign */
const t = require('tap')
const EventEmitter = require('events')
const os = require('os')
const fs = require('fs')
const path = require('path')

const { real: mockNpm } = require('../../fixtures/mock-npm')

// generic error to be used in tests
const err = Object.assign(new Error('ERROR'), { code: 'ERROR' })
err.stack = 'Error: ERROR'

const redactCwd = (path) => {
  const normalizePath = p => p
    .replace(/\\+/g, '/')
    .replace(/\r\n/g, '\n')
  return normalizePath(path)
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')
}

t.cleanSnapshot = (str) => redactCwd(str)

const cacheFolder = t.testdir({})
const logFile = path.resolve(cacheFolder, '_logs', 'expecteddate-debug.log')
const timingFile = path.resolve(cacheFolder, '_timing.json')

const { npm } = mockNpm(t)

t.before(async () => {
  npm.version = '1.0.0'
  await npm.load()
  npm.config.set('cache', cacheFolder)
})

t.test('bootstrap tap before cutting off process ref', (t) => {
  t.ok('ok')
  t.end()
})

// cut off process from script so that it won't quit the test runner
// while trying to run through the myriad of cases
const _process = process
process = Object.assign(
  new EventEmitter(),
  {
    argv: ['/node', ..._process.argv.slice(1)],
    cwd: _process.cwd,
    env: _process.env,
    version: 'v1.0.0',
    exit: (code) => {
      process.exitCode = code || process.exitCode || 0
      process.emit('exit', process.exitCode)
    },
    stdout: { write (_, cb) {
      cb()
    } },
    stderr: { write () {} },
    hrtime: _process.hrtime,
  }
)

const osType = os.type
const osRelease = os.release
// overrides OS type/release for cross platform snapshots
os.type = () => 'Foo'
os.release = () => '1.0.0'

// generates logfile name with mocked date
const _toISOString = Date.prototype.toISOString
Date.prototype.toISOString = () => 'expecteddate'

const consoleError = console.error
const errors = []
console.error = (err) => {
  errors.push(err)
}
t.teardown(() => {
  os.type = osType
  os.release = osRelease
  // needs to put process back in its place in order for tap to exit properly
  process = _process
  Date.prototype.toISOString = _toISOString
  console.error = consoleError
})

t.afterEach(() => {
  errors.length = 0
  npm.log.level = 'silent'
  // clear out the 'A complete log' message
  npm.log.record.length = 0
  delete process.exitCode
})

const mocks = {
  '../../../lib/utils/error-message.js': (err) => ({
    ...err,
    summary: [['ERR', err.message]],
    detail: [['ERR', err.message]],
  }),
}

const exitHandler = t.mock('../../../lib/utils/exit-handler.js', mocks)
exitHandler.setNpm(npm)

t.test('exit handler never called - loglevel silent', (t) => {
  npm.log.level = 'silent'
  process.emit('exit', 1)
  const logData = fs.readFileSync(logFile, 'utf8')
  t.match(logData, 'Exit handler never called!')
  t.match(errors, [''], 'logs one empty string to console.error')
  t.end()
})

t.test('exit handler never called - loglevel notice', (t) => {
  npm.log.level = 'notice'
  process.emit('exit', 1)
  const logData = fs.readFileSync(logFile, 'utf8')
  t.match(logData, 'Exit handler never called!')
  t.match(errors, ['', ''], 'logs two empty strings to console.error')
  t.end()
})

t.test('handles unknown error', (t) => {
  t.plan(2)

  npm.log.level = 'notice'

  process.once('timeEnd', (msg) => {
    t.equal(msg, 'npm', 'should trigger timeEnd for npm')
  })

  exitHandler(err)
  const logData = fs.readFileSync(logFile, 'utf8')
  t.matchSnapshot(
    logData,
    'should have expected log contents for unknown error'
  )
  t.end()
})

t.test('fail to write logfile', (t) => {
  t.plan(1)

  t.teardown(() => {
    npm.config.set('cache', cacheFolder)
  })

  const badDir = t.testdir({
    _logs: 'is a file',
  })

  npm.config.set('cache', badDir)

  t.doesNotThrow(
    () => exitHandler(err),
    'should not throw on cache write failure'
  )
})

t.test('console.log output using --json', (t) => {
  t.plan(1)

  npm.config.set('json', true)
  t.teardown(() => {
    npm.config.set('json', false)
  })

  exitHandler(new Error('Error: EBADTHING Something happened'))
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

t.test('throw a non-error obj', (t) => {
  t.plan(2)

  const weirdError = {
    code: 'ESOMETHING',
    message: 'foo bar',
  }

  process.once('exit', code => {
    t.equal(code, 1, 'exits with exitCode 1')
  })
  exitHandler(weirdError)
  t.match(
    npm.log.record.find(r => r.level === 'error'),
    { message: 'foo bar' }
  )
})

t.test('throw a string error', (t) => {
  t.plan(2)
  const error = 'foo bar'

  process.once('exit', code => {
    t.equal(code, 1, 'exits with exitCode 1')
  })
  exitHandler(error)
  t.match(
    npm.log.record.find(r => r.level === 'error'),
    { message: 'foo bar' }
  )
})

t.test('update notification', (t) => {
  const updateMsg = 'you should update npm!'
  npm.updateNotification = updateMsg
  npm.log.level = 'silent'

  t.teardown(() => {
    delete npm.updateNotification
  })

  exitHandler()
  t.match(
    npm.log.record.find(r => r.level === 'notice'),
    { message: 'you should update npm!' }
  )
  t.end()
})

t.test('npm.config not ready', (t) => {
  t.plan(1)

  const { npm: unloaded } = mockNpm(t)

  t.teardown(() => {
    exitHandler.setNpm(npm)
  })

  exitHandler.setNpm(unloaded)

  exitHandler()
  t.match(
    errors[0],
    /Error: Exit prior to config file resolving./,
    'should exit with config error msg'
  )
  t.end()
})

t.test('timing', (t) => {
  npm.config.set('timing', true)

  t.teardown(() => {
    fs.unlinkSync(timingFile)
    npm.config.set('timing', false)
  })

  exitHandler()
  const timingData = JSON.parse(fs.readFileSync(timingFile, 'utf8'))
  t.match(timingData, { version: '1.0.0', 'config:load:defaults': Number })
  t.end()
})

t.test('timing - with error', (t) => {
  npm.config.set('timing', true)

  t.teardown(() => {
    fs.unlinkSync(timingFile)
    npm.config.set('timing', false)
  })

  exitHandler(err)
  const timingData = JSON.parse(fs.readFileSync(timingFile, 'utf8'))
  t.match(timingData, { version: '1.0.0', 'config:load:defaults': Number })
  t.end()
})

t.test('uses code from errno', (t) => {
  t.plan(1)

  process.once('exit', code => {
    t.equal(code, 127, 'should set exitCode from errno')
  })
  exitHandler(Object.assign(
    new Error('Error with errno'),
    {
      errno: 127,
    }
  ))
})

t.test('uses code from number', (t) => {
  t.plan(1)

  process.once('exit', code => {
    t.equal(code, 404, 'should set exitCode from a number')
  })
  exitHandler(Object.assign(
    new Error('Error with code type number'),
    {
      code: 404,
    }
  ))
})

t.test('call exitHandler with no error', (t) => {
  t.plan(1)
  process.once('exit', code => {
    t.equal(code, 0, 'should end up with exitCode 0 (default)')
  })
  exitHandler()
})

t.test('defaults to log error msg if stack is missing', (t) => {
  const { npm: unloaded } = mockNpm(t)

  t.teardown(() => {
    exitHandler.setNpm(npm)
  })

  exitHandler.setNpm(unloaded)
  const noStackErr = Object.assign(
    new Error('Error with no stack'),
    {
      code: 'ENOSTACK',
      errno: 127,
    }
  )
  delete noStackErr.stack

  exitHandler(noStackErr)
  t.equal(errors[0], 'Error with no stack', 'should use error msg')
  t.end()
})

t.test('exits uncleanly when only emitting exit event', (t) => {
  t.plan(2)

  npm.log.level = 'silent'
  process.emit('exit')
  const logData = fs.readFileSync(logFile, 'utf8')
  t.match(logData, 'Exit handler never called!')
  t.match(process.exitCode, 1, 'exitCode coerced to 1')
  t.end()
})

t.test('do no fancy handling for shellouts', t => {
  const { command } = npm
  const LOG_RECORD = []
  npm.command = 'exec'

  t.teardown(() => {
    npm.command = command
  })
  t.beforeEach(() => LOG_RECORD.length = 0)

  const loudNoises = () => npm.log.record
    .filter(({ level }) => ['warn', 'error'].includes(level))

  t.test('shellout with a numeric error code', t => {
    t.plan(2)
    process.once('exit', code => {
      t.equal(code, 5, 'got expected exit code')
    })
    exitHandler(Object.assign(new Error(), { code: 5 }))
    t.strictSame(loudNoises(), [], 'no noisy warnings')
  })

  t.test('shellout without a numeric error code (something in npm)', t => {
    t.plan(2)
    process.once('exit', code => {
      t.equal(code, 1, 'got expected exit code')
    })
    exitHandler(Object.assign(new Error(), { code: 'banana stand' }))
    // should log some warnings and errors, because something weird happened
    t.strictNotSame(loudNoises(), [], 'bring the noise')
    t.end()
  })

  t.test('shellout with code=0 (extra weird?)', t => {
    t.plan(2)
    process.once('exit', code => {
      t.equal(code, 1, 'got expected exit code')
    })
    exitHandler(Object.assign(new Error(), { code: 0 }))
    t.strictNotSame(loudNoises(), [], 'bring the noise')
  })

  t.end()
})
