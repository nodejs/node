const fs = require('node:fs')
const { join, resolve } = require('node:path')
const EventEmitter = require('node:events')
const os = require('node:os')
const t = require('tap')
const fsMiniPass = require('fs-minipass')
const { output, time } = require('proc-log')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const mockGlobals = require('@npmcli/mock-globals')
const { cleanCwd, cleanDate } = require('../../fixtures/clean-snapshot')
const tmock = require('../../fixtures/tmock')
const { version: NPM_VERSION } = require('../../../package.json')

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
  // logfile cleaning is not awaited so it races with the process.exit
  // in this test and can cause snapshot failures so we always remove them
  .replace(/.*silly logfile.*cleaning.*\n/gm, '')
  .replace(/(Completed in )\d+(ms)/g, '$1{TIME}$2')
  .replace(/(removing )\d+( files)/g, '$1${NUM}2')
  .replaceAll(`node ${process.version}`, '{NODE-VERSION}')
  .replaceAll(`${os.type()} ${os.release()}`, '{OS}')
  .replaceAll(`v${NPM_VERSION}`, '{NPM-VERSION}')

// cut off process from script so that it won't quit the test runner
// while trying to run through the myriad of cases.  need to make it
// have all the functions signal-exit relies on so that it doesn't
// nerf itself, thinking global.process is broken or gone.
mockGlobals(t, {
  process: Object.assign(new EventEmitter(), {
    // these are process properties that are needed in the running code and tests
    // eslint-disable-next-line max-len
    ...pick(process, 'version', 'execPath', 'stdout', 'stderr', 'stdin', 'cwd', 'chdir', 'env', 'umask'),
    argv: ['/node', ...process.argv.slice(1)],
    kill: () => {},
    reallyExit: (code) => process.exit(code),
    pid: 123456,
    exit: (code) => {
      process.exitCode = code || process.exitCode || 0
      process.emit('exit', process.exitCode)
    },
  }),
}, { replace: true })

const mockExitHandler = async (t, { config, mocks, files, ...opts } = {}) => {
  const errors = []

  const { npm, ...rest } = await loadMockNpm(t, {
    ...opts,
    mocks,
    config: (dirs) => ({
      loglevel: 'notice',
      ...(typeof config === 'function' ? config(dirs) : config),
    }),
    globals: {
      'console.error': (err) => errors.push(err),
    },
  })

  const exitHandler = tmock(t, '{LIB}/cli/exit-handler.js', {
    '{LIB}/utils/error-message.js': (err) => ({
      summary: [['ERR SUMMARY', err.message]],
      detail: [['ERR DETAIL', err.message]],
      ...(files ? { files } : {}),
      json: {
        code: err.code,
        summary: err.message,
        detail: err.message,
      },
    }),
    ...mocks,
  })

  if (npm) {
    exitHandler.setNpm(npm)
  }

  t.teardown(() => {
    process.removeAllListeners('exit')
  })

  return {
    ...rest,
    errors: () => [
      ...rest.outputErrors,
      ...errors,
    ],
    npm,
    // Make it async to make testing ergonomics a little easier so we dont need
    // to t.plan() every test to make sure we get process.exit called.
    exitHandler: (...args) => new Promise(res => {
      process.once('exit', res)
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
  const { exitHandler, debugFile, logs } = await mockExitHandler(t, {
    config: { loglevel: 'silly', timing: true },
  })

  await exitHandler(err('Unknown error', 'ECODE'))

  const fileLogs = await debugFile()
  const fileLines = fileLogs.split('\n')

  const lineNumber = /^(\d+)\s/
  const lastLog = fileLines[fileLines.length - 1].match(lineNumber)[1]

  t.equal(process.exitCode, 1)

  logs.forEach((logItem, i) => {
    const logLines = logItem.split('\n').map(l => `${i} ${l}`)
    for (const line of logLines) {
      t.match(fileLogs.trim(), line, 'log appears in debug file')
    }
  })

  t.equal(logs.length, parseInt(lastLog) + 1)
  t.match(logs.error, [
    'code ECODE',
    'ERR SUMMARY Unknown error',
    'ERR DETAIL Unknown error',
  ])
  t.match(fileLogs, /\d+ error code ECODE/)
  t.match(fileLogs, /\d+ error ERR SUMMARY Unknown error/)
  t.match(fileLogs, /\d+ error ERR DETAIL Unknown error/)
  t.matchSnapshot(logs, 'logs')
  t.matchSnapshot(fileLines.map(l => l.replace(lineNumber, 'XX ')), 'debug file contents')
})

t.test('exit handler never called - loglevel silent', async (t) => {
  const { logs, errors } = await mockExitHandler(t, {
    config: { loglevel: 'silent' },
  })
  process.emit('exit', 1)
  t.strictSame(logs.error, [])
  t.strictSame(errors(), [''], 'one empty string')
})

t.test('exit handler never called - loglevel notice', async (t) => {
  const { logs, errors } = await mockExitHandler(t)
  process.emit('exit', 1)
  t.equal(process.exitCode, 1)
  t.match(logs.error, [
    'Exit handler never called!',
    /error with npm itself/,
  ])
  t.strictSame(errors(), ['', ''], 'two empty string on output')
})

t.test('exit handler never called - no npm', async (t) => {
  const { logs, errors } = await mockExitHandler(t, { init: false })
  process.emit('exit', 1)
  t.equal(process.exitCode, 1)
  t.strictSame(logs.error, [])
  t.strictSame(errors(), [''], 'one empty string')
})

t.test('exit handler called - no npm', async (t) => {
  const { exitHandler, errors } = await mockExitHandler(t, { init: false })
  await exitHandler()
  t.equal(process.exitCode, 1)
  t.equal(errors().length, 1)
  t.match(errors(), [/Error: Exit prior to setting npm in exit handler/])
})

t.test('exit handler called - no npm with error', async (t) => {
  const { exitHandler, errors } = await mockExitHandler(t, { init: false })
  await exitHandler(err('something happened'))
  t.equal(process.exitCode, 1)
  t.equal(errors().length, 1)
  t.match(errors(), [/Error: something happened/])
})

t.test('exit handler called - no npm with error without stack', async (t) => {
  const { exitHandler, errors } = await mockExitHandler(t, { init: false })
  await exitHandler(err('something happened', {}, true))
  t.equal(process.exitCode, 1)
  t.equal(errors().length, 1)
  t.match(errors(), [/something happened/])
})

t.test('standard output using --json', async (t) => {
  const { exitHandler, outputs } = await mockExitHandler(t, {
    config: { json: true },
  })

  await exitHandler(err('Error: EBADTHING Something happened'))

  t.equal(process.exitCode, 1)
  t.same(
    JSON.parse(outputs[0]),
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

t.test('merges output buffers errors with --json', async (t) => {
  const { exitHandler, outputs } = await mockExitHandler(t, {
    config: { json: true },
  })

  output.buffer({ output_data: 1 })
  output.buffer(JSON.stringify({ more_data: 2 }))
  output.buffer('not json, will be ignored')

  await exitHandler(err('Error: EBADTHING Something happened'))

  t.equal(process.exitCode, 1)
  t.same(
    JSON.parse(outputs[0]),
    {
      output_data: 1,
      more_data: 2,
      error: {
        code: 'EBADTHING', // should default error code to E[A-Z]+
        summary: 'Error: EBADTHING Something happened',
        detail: 'Error: EBADTHING Something happened',
      },
    },
    'should output expected json output'
  )
})

t.test('output buffer without json', async (t) => {
  const { exitHandler, outputs, logs } = await mockExitHandler(t)

  output.buffer('output_data')
  output.buffer('more_data')

  await exitHandler(err('Error: EBADTHING Something happened'))

  t.equal(process.exitCode, 1)
  t.same(
    outputs,
    ['output_data', 'more_data'],
    'should output expected output'
  )
  t.match(logs.error, ['code EBADTHING'])
})

t.test('throw a non-error obj', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler({
    code: 'ESOMETHING',
    message: 'foo bar',
  })

  t.equal(process.exitCode, 1)
  t.match(logs.error, [
    "weird error { code: 'ESOMETHING', message: 'foo bar' }",
  ])
})

t.test('throw a string error', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler('foo bar')

  t.equal(process.exitCode, 1)
  t.match(logs.error, [
    'foo bar',
  ])
})

t.test('update notification - shows even with loglevel error', async (t) => {
  const { exitHandler, logs, npm } = await mockExitHandler(t, {
    config: { loglevel: 'error' },
  })
  npm.updateNotification = 'you should update npm!'

  await exitHandler()

  t.match(logs.notice, [
    'you should update npm!',
  ])
})

t.test('update notification - hidden with silent', async (t) => {
  const { exitHandler, logs, npm } = await mockExitHandler(t, {
    config: { loglevel: 'silent' },
  })
  npm.updateNotification = 'you should update npm!'

  await exitHandler()

  t.strictSame(logs.notice, [])
})

t.test('npm.config not ready', async (t) => {
  const { exitHandler, logs, errors } = await mockExitHandler(t, {
    load: false,
  })

  await exitHandler()

  t.equal(process.exitCode, 1)
  t.equal(errors().length, 1)
  t.match(errors(), [
    /Error: Exit prior to config file resolving./,
  ], 'should exit with config error msg')
  t.strictSame(logs, [], 'no logs if it doesnt load')
})

t.test('no logs dir', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t, {
    config: { 'logs-max': 0 },
  })
  await exitHandler(new Error())

  t.match(logs.error[2],
    'Log files were not written due to the config logs-max=0')
  t.match(logs.filter((l) => l.includes('npm.load.mkdirplogs')), [])
})

t.test('timers fail to write', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t, {
    config: (dirs) => ({
      'logs-dir': resolve(dirs.prefix, 'LOGS_DIR'),
      timing: true,
    }),
    mocks: {
      // we want the fs.writeFileSync in the Timers class to fail
      '{LIB}/utils/timers.js': tmock(t, '{LIB}/utils/timers.js', {
        'node:fs': {
          ...fs,
          writeFileSync: (file, ...rest) => {
            if (file.includes('LOGS_DIR')) {
              throw new Error('err')
            }

            return fs.writeFileSync(file, ...rest)
          },
        },
      }),
    },
  })

  await exitHandler(new Error())

  t.match(logs.warn[0], `timing could not write timing file: Error: err`)
})

t.test('log files fail to write', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t, {
    config: (dirs) => ({
      'logs-dir': resolve(dirs.prefix, 'LOGS_DIR'),
    }),
    mocks: {
      // we want the fsMiniPass.WriteStreamSync in the LogFile class to fail
      '{LIB}/utils/log-file.js': tmock(t, '{LIB}/utils/log-file.js', {
        'fs-minipass': {
          ...fsMiniPass,
          WriteStreamSync: (file) => {
            if (file.includes('LOGS_DIR')) {
              throw new Error('err')
            }
          },
        },
      }),
    },
  })

  await exitHandler(new Error())

  t.match(logs.error[2], `error writing to the directory`)
})

t.test('files from error message', async (t) => {
  const { exitHandler, logs, cache } = await mockExitHandler(t, {
    files: [
      ['error-file.txt', '# error file content'],
    ],
  })

  await exitHandler(err('Error message'))

  const logFiles = fs.readdirSync(join(cache, '_logs'))
  const errorFileName = logFiles.find(f => f.endsWith('error-file.txt'))
  const errorFile = fs.readFileSync(join(cache, '_logs', errorFileName)).toString()

  t.match(logs[2], /For a full report see:\n.*-error-file\.txt/)
  t.match(errorFile, '# error file content')
  t.match(errorFile, 'Log files:')
})

t.test('files from error message with error', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t, {
    config: (dirs) => ({
      'logs-dir': resolve(dirs.prefix, 'LOGS_DIR'),
    }),
    files: [
      ['error-file.txt', '# error file content'],
    ],
    mocks: {
      'node:fs': {
        ...fs,
        writeFileSync: (dir) => {
          if (dir.includes('LOGS_DIR') && dir.endsWith('error-file.txt')) {
            throw new Error('err')
          }
        },
      },
    },
  })

  await exitHandler(err('Error message'))

  t.match(logs.warn[0], /Could not write error message to.*error-file\.txt.*err/)
})

t.test('timing with no error', async (t) => {
  const { exitHandler, timingFile, npm, logs } = await mockExitHandler(t, {
    config: { timing: true, loglevel: 'silly' },
  })

  await exitHandler()
  const timingFileData = await timingFile()

  t.equal(process.exitCode, 0)

  const msg = logs.info.byTitle('timing')[0]
  t.match(msg, /Timing info written to:/)

  t.match(timingFileData, {
    metadata: {
      command: [],
      version: npm.version,
      logfiles: [String],
    },
    timers: {
      'npm:load': Number,
      npm: Number,
    },
  })
})

t.test('timing message hidden by loglevel', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t, {
    config: { timing: true, loglevel: 'notice' },
  })

  await exitHandler()

  t.equal(process.exitCode, 0)

  t.strictSame(logs.info, [], 'no log message')
})

t.test('unfinished timers', async (t) => {
  const { exitHandler, timingFile, npm } = await mockExitHandler(t, {
    config: { timing: true },
  })

  time.start('foo')
  time.start('bar')

  await exitHandler()
  const timingFileData = await timingFile()

  t.equal(process.exitCode, 0)
  t.match(timingFileData, {
    metadata: {
      command: [],
      version: npm.version,
      logfiles: [String],
    },
    timers: {
      npm: Number,
    },
    unfinishedTimers: {
      foo: [Number, Number],
      bar: [Number, Number],
    },
  })
})

t.test('uses code from errno', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler(err('Error with errno', { errno: 127 }))
  t.equal(process.exitCode, 127)
  t.match(logs.error, ['errno 127'])
})

t.test('uses code from number', async (t) => {
  const { exitHandler, logs } = await mockExitHandler(t)

  await exitHandler(err('Error with code type number', 404))
  t.equal(process.exitCode, 404)
  t.match(logs.error, ['code 404'])
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
  t.match(logs.error, keys.map((k) => `${k} ${k}-hey`), 'all special keys get logged')
})

t.test('verbose logs replace info on err props', async t => {
  const { exitHandler, logs } = await mockExitHandler(t, {
    config: { loglevel: 'verbose' },
  })

  const keys = ['type', 'stack', 'pkgid']
  const properties = keys.reduce((acc, k) => {
    acc[k] = `${k}-https://user:pass@registry.npmjs.org/`
    return acc
  }, {})

  await exitHandler(err('Error with code type number', properties))
  t.equal(process.exitCode, 1)
  t.match(
    logs.verbose.filter(l => !/^(logfile|title|argv)/.test(l)),
    keys.map((k) => `${k} ${k}-https://user:***@registry.npmjs.org/`),
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
  t.strictSame(errors(), ['Error with no stack'], 'should use error msg')
  t.strictSame(logs.error, [])
})

t.test('exits uncleanly when only emitting exit event', async (t) => {
  const { logs } = await mockExitHandler(t)

  process.emit('exit')

  t.match(logs.error, ['Exit handler never called!'])
  t.equal(process.exitCode, 1, 'exitCode coerced to 1')
})

t.test('do no fancy handling for shellouts', async t => {
  const mockShelloutExit = (t) => mockExitHandler(t, {
    command: 'exec',
    exec: true,
    argv: ['-c', 'exit'],
    config: {
      timing: false,
    },
  })

  t.test('shellout with a numeric error code', async t => {
    const { exitHandler, logs, errors } = await mockShelloutExit(t)
    await exitHandler(err('', 5))
    t.equal(process.exitCode, 5, 'got expected exit code')
    t.strictSame(logs.error, [], 'no noisy warnings')
    t.strictSame(logs.warn, [], 'no noisy warnings')
    t.strictSame(errors(), [])
  })

  t.test('shellout without a numeric error code (something in npm)', async t => {
    const { exitHandler, logs, errors } = await mockShelloutExit(t)
    await exitHandler(err('', 'banana stand'))
    t.equal(process.exitCode, 1, 'got expected exit code')
    // should log some warnings and errors, because something weird happened
    t.strictNotSame(logs.error, [], 'bring the noise')
    t.strictSame(errors(), [''])
  })

  t.test('shellout with code=0 (extra weird?)', async t => {
    const { exitHandler, logs, errors } = await mockShelloutExit(t)
    await exitHandler(Object.assign(new Error(), { code: 0 }))
    t.equal(process.exitCode, 1, 'got expected exit code')
    t.strictNotSame(logs.error, [], 'bring the noise')
    t.strictSame(errors(), [''])
  })
})
