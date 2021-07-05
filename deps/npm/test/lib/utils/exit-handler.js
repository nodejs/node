/* eslint-disable no-extend-native */
/* eslint-disable no-global-assign */
const EventEmitter = require('events')
const writeFileAtomic = require('write-file-atomic')
const t = require('tap')

// NOTE: Although these unit tests may look like the rest on the surface,
// they are in fact very special due to the amount of things hooking directly
// to global process and variables defined in the module scope. That makes
// for tests that are very interdependent and their order are important.

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

// internal modules mocks
const cacheFolder = t.testdir({})
const config = {
  values: {
    cache: cacheFolder,
    timing: true,
  },
  loaded: true,
  updateNotification: null,
  get (key) {
    return this.values[key]
  },
}

const npm = {
  version: '1.0.0',
  config,
  shelloutCommands: ['exec', 'run-script'],
}

const npmlog = {
  disableProgress: () => null,
  log (level, ...args) {
    this.record.push({
      id: this.record.length,
      level,
      message: args.reduce((res, i) => `${res} ${i.message ? i.message : i}`, ''),
      prefix: level !== 'verbose' ? 'foo' : '',
    })
  },
  error (...args) {
    this.log('error', ...args)
  },
  info (...args) {
    this.log('info', ...args)
  },
  level: 'silly',
  levels: {
    silly: 0,
    verbose: 1,
    info: 2,
    error: 3,
    silent: 4,
  },
  notice (...args) {
    this.log('notice', ...args)
  },
  record: [],
  verbose (...args) {
    this.log('verbose', ...args)
  },
}

// overrides OS type/release for cross platform snapshots
const os = require('os')
os.type = () => 'Foo'
os.release = () => '1.0.0'

// bootstrap tap before cutting off process ref
t.test('ok', (t) => {
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
    exit () {},
    exitCode: 0,
    version: 'v1.0.0',
    stdout: { write (_, cb) {
      cb()
    } },
    stderr: { write () {} },
    hrtime: _process.hrtime,
  }
)
// needs to put process back in its place
// in order for tap to exit properly
t.teardown(() => {
  process = _process
})

t.afterEach(() => {
  // clear out the 'A complete log' message
  npmlog.record.length = 0
})

const mocks = {
  npmlog,
  '../../../lib/utils/error-message.js': (err) => ({
    ...err,
    summary: [['ERR', err.message]],
    detail: [['ERR', err.message]],
  }),
}

let exitHandler = t.mock('../../../lib/utils/exit-handler.js', mocks)
exitHandler.setNpm(npm)

t.test('default exit code', (t) => {
  t.plan(1)

  // manually simulate timing handlers
  process.emit('timing', 'foo', 1)
  process.emit('timing', 'foo', 2)

  // generates logfile name with mocked date
  const _toISOString = Date.prototype.toISOString
  Date.prototype.toISOString = () => 'expecteddate'

  npmlog.level = 'silent'
  const _exit = process.exit
  process.exit = (code) => {
    t.equal(code, 1, 'should default to error code 1')
  }

  // skip console.error logs
  const _error = console.error
  console.error = () => null

  process.emit('exit', 1)

  t.teardown(() => {
    npmlog.level = 'silly'
    process.exit = _exit
    console.error = _error
    Date.prototype.toISOString = _toISOString
  })
})

t.test('handles unknown error', (t) => {
  t.plan(2)

  const _toISOString = Date.prototype.toISOString
  Date.prototype.toISOString = () => 'expecteddate'

  const sync = writeFileAtomic.sync
  writeFileAtomic.sync = (filename, content) => {
    t.equal(
      redactCwd(filename),
      '{CWD}/test/lib/utils/tap-testdir-exit-handler/_logs/expecteddate-debug.log',
      'should use expected log filename'
    )
    t.matchSnapshot(
      content,
      'should have expected log contents for unknown error'
    )
  }

  exitHandler(err)

  t.teardown(() => {
    writeFileAtomic.sync = sync
    Date.prototype.toISOString = _toISOString
  })
  t.end()
})

t.test('npm.config not ready', (t) => {
  t.plan(1)

  config.loaded = false

  const _error = console.error
  console.error = (msg) => {
    t.match(
      msg,
      /Error: Exit prior to config file resolving./,
      'should exit with config error msg'
    )
  }

  exitHandler()

  t.teardown(() => {
    console.error = _error
    config.loaded = true
  })
})

t.test('fail to write logfile', (t) => {
  t.plan(1)

  const badDir = t.testdir({
    _logs: 'is a file',
  })

  config.values.cache = badDir

  t.teardown(() => {
    config.values.cache = cacheFolder
  })

  t.doesNotThrow(
    () => exitHandler(err),
    'should not throw on cache write failure'
  )
})

t.test('console.log output using --json', (t) => {
  t.plan(1)

  config.values.json = true

  const _error = console.error
  console.error = (jsonOutput) => {
    t.same(
      JSON.parse(jsonOutput),
      {
        error: {
          code: 'EBADTHING', // should default error code to E[A-Z]+
          summary: 'Error: EBADTHING Something happened',
          detail: 'Error: EBADTHING Something happened',
        },
      },
      'should output expected json output'
    )
  }

  exitHandler(new Error('Error: EBADTHING Something happened'))

  t.teardown(() => {
    console.error = _error
    delete config.values.json
  })
})

t.test('throw a non-error obj', (t) => {
  t.plan(3)

  const weirdError = {
    code: 'ESOMETHING',
    message: 'foo bar',
  }

  const _logError = npmlog.error
  npmlog.error = (title, err) => {
    t.equal(title, 'weird error', 'should name it a weird error')
    t.same(err, weirdError, 'should log given weird error')
  }

  const _exit = process.exit
  process.exit = (code) => {
    t.equal(code, 1, 'should exit with code 1')
  }

  exitHandler(weirdError)

  t.teardown(() => {
    process.exit = _exit
    npmlog.error = _logError
  })
})

t.test('throw a string error', (t) => {
  t.plan(3)

  const error = 'foo bar'

  const _logError = npmlog.error
  npmlog.error = (title, err) => {
    t.equal(title, '', 'should have an empty name ref')
    t.same(err, 'foo bar', 'should log string error')
  }

  const _exit = process.exit
  process.exit = (code) => {
    t.equal(code, 1, 'should exit with code 1')
  }

  exitHandler(error)

  t.teardown(() => {
    process.exit = _exit
    npmlog.error = _logError
  })
})

t.test('update notification', (t) => {
  t.plan(2)

  const updateMsg = 'you should update npm!'
  npm.updateNotification = updateMsg

  const _notice = npmlog.notice
  npmlog.notice = (prefix, msg) => {
    t.equal(prefix, '', 'should have no prefix')
    t.equal(msg, updateMsg, 'should show update message')
  }

  exitHandler(err)

  t.teardown(() => {
    npmlog.notice = _notice
    delete npm.updateNotification
  })
})

t.test('on exit handler', (t) => {
  t.plan(2)

  const _exit = process.exit
  process.exit = (code) => {
    t.equal(code, 1, 'should default to error code 1')
  }

  process.once('timeEnd', (msg) => {
    t.equal(msg, 'npm', 'should trigger timeEnd for npm')
  })

  // skip console.error logs
  const _error = console.error
  console.error = () => null

  process.emit('exit', 1)

  t.teardown(() => {
    console.error = _error
    process.exit = _exit
  })
})

t.test('it worked', (t) => {
  t.plan(2)

  config.values.timing = false

  const _exit = process.exit
  process.exit = (code) => {
    process.exit = _exit
    t.notOk(code, 'should exit with no code')

    const _info = npmlog.info
    npmlog.info = (msg) => {
      npmlog.info = _info
      t.equal(msg, 'ok', 'should log ok if "it worked"')
    }

    process.emit('exit', 0)
  }

  t.teardown(() => {
    process.exit = _exit
    config.values.timing = true
  })

  exitHandler()
})

t.test('uses code from errno', (t) => {
  t.plan(1)

  exitHandler = t.mock('../../../lib/utils/exit-handler.js', mocks)
  exitHandler.setNpm(npm)

  npmlog.level = 'silent'
  const _exit = process.exit
  process.exit = (code) => {
    t.equal(code, 127, 'should use set errno')
  }

  exitHandler(Object.assign(
    new Error('Error with errno'),
    {
      errno: 127,
    }
  ))

  t.teardown(() => {
    npmlog.level = 'silly'
    process.exit = _exit
  })
})

t.test('uses exitCode as code if using a number', (t) => {
  t.plan(1)

  exitHandler = t.mock('../../../lib/utils/exit-handler.js', mocks)
  exitHandler.setNpm(npm)

  npmlog.level = 'silent'
  const _exit = process.exit
  process.exit = (code) => {
    t.equal(code, 404, 'should use code if a number')
  }

  exitHandler(Object.assign(
    new Error('Error with code type number'),
    {
      code: 404,
    }
  ))

  t.teardown(() => {
    npmlog.level = 'silly'
    process.exit = _exit
  })
})

t.test('call exitHandler with no error', (t) => {
  t.plan(1)

  exitHandler = t.mock('../../../lib/utils/exit-handler.js', mocks)
  exitHandler.setNpm(npm)

  const _exit = process.exit
  process.exit = (code) => {
    t.equal(code, undefined, 'should exit with code undefined')
  }

  t.teardown(() => {
    process.exit = _exit
  })

  exitHandler()
})

t.test('exit handler called twice', (t) => {
  t.plan(2)

  const _verbose = npmlog.verbose
  npmlog.verbose = (key, value) => {
    t.equal(key, 'stack', 'should log stack in verbose level')
    t.match(
      value,
      /Error: Exit handler called more than once./,
      'should have expected error msg'
    )
    npmlog.verbose = _verbose
  }

  exitHandler()
})

t.test('defaults to log error msg if stack is missing', (t) => {
  t.plan(1)

  const noStackErr = Object.assign(
    new Error('Error with no stack'),
    {
      code: 'ENOSTACK',
      errno: 127,
    }
  )
  delete noStackErr.stack

  npm.config.loaded = false

  const _error = console.error
  console.error = (msg) => {
    console.error = _error
    npm.config.loaded = true
    t.equal(msg, 'Error with no stack', 'should use error msg')
  }

  exitHandler(noStackErr)
})

t.test('exits cleanly when emitting exit event', (t) => {
  t.plan(1)

  npmlog.level = 'silent'
  const _exit = process.exit
  process.exit = (code) => {
    process.exit = _exit
    t.same(code, null, 'should exit with code null')
  }

  t.teardown(() => {
    process.exit = _exit
    npmlog.level = 'silly'
  })

  process.emit('exit')
})

t.test('do no fancy handling for shellouts', t => {
  const { exit } = process
  const { command } = npm
  const { log } = npmlog
  const LOG_RECORD = []
  t.teardown(() => {
    npmlog.log = log
    process.exit = exit
    npm.command = command
  })

  npmlog.log = function (level, ...args) {
    log.call(this, level, ...args)
    LOG_RECORD.push(npmlog.record[npmlog.record.length - 1])
  }

  npm.command = 'exec'

  let EXPECT_EXIT = 0
  process.exit = code => {
    t.equal(code, EXPECT_EXIT, 'got expected exit code')
    EXPECT_EXIT = 0
  }
  t.beforeEach(() => LOG_RECORD.length = 0)

  const loudNoises = () => LOG_RECORD
    .filter(({ level }) => ['warn', 'error'].includes(level))

  t.test('shellout with a numeric error code', t => {
    EXPECT_EXIT = 5
    exitHandler(Object.assign(new Error(), { code: 5 }))
    t.equal(EXPECT_EXIT, 0, 'called process.exit')
    // should log no warnings or errors, verbose/silly is fine.
    t.strictSame(loudNoises(), [], 'no noisy warnings')
    t.end()
  })

  t.test('shellout without a numeric error code (something in npm)', t => {
    EXPECT_EXIT = 1
    exitHandler(Object.assign(new Error(), { code: 'banana stand' }))
    t.equal(EXPECT_EXIT, 0, 'called process.exit')
    // should log some warnings and errors, because something weird happened
    t.strictNotSame(loudNoises(), [], 'bring the noise')
    t.end()
  })

  t.test('shellout with code=0 (extra weird?)', t => {
    EXPECT_EXIT = 1
    exitHandler(Object.assign(new Error(), { code: 0 }))
    t.equal(EXPECT_EXIT, 0, 'called process.exit')
    // should log some warnings and errors, because something weird happened
    t.strictNotSame(loudNoises(), [], 'bring the noise')
    t.end()
  })

  t.end()
})
