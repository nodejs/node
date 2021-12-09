const os = require('os')
const log = require('./log-shim.js')

const errorMessage = require('./error-message.js')
const replaceInfo = require('./replace-info.js')

const messageText = msg => msg.map(line => line.slice(1).join(' ')).join('\n')

let npm = null // set by the cli
let exitHandlerCalled = false
let showLogFileMessage = false

process.on('exit', code => {
  log.disableProgress()

  // process.emit is synchronous, so the timeEnd handler will run before the
  // unfinished timer check below
  process.emit('timeEnd', 'npm')

  const hasNpm = !!npm
  const hasLoadedNpm = hasNpm && npm.config.loaded

  // Unfinished timers can be read before config load
  if (hasNpm) {
    for (const [name, timer] of npm.unfinishedTimers) {
      log.verbose('unfinished npm timer', name, timer)
    }
  }

  if (!code) {
    log.info('ok')
  } else {
    log.verbose('code', code)
  }

  if (!exitHandlerCalled) {
    process.exitCode = code || 1
    log.error('', 'Exit handler never called!')
    console.error('')
    log.error('', 'This is an error with npm itself. Please report this error at:')
    log.error('', '    <https://github.com/npm/cli/issues>')
    showLogFileMessage = true
  }

  // In timing mode we always show the log file message
  if (hasLoadedNpm && npm.config.get('timing')) {
    showLogFileMessage = true
  }

  // npm must be loaded to know where the log file was written
  if (showLogFileMessage && hasLoadedNpm) {
    // just a line break
    if (log.levels[log.level] <= log.levels.error) {
      console.error('')
    }

    log.error(
      '',
      [
        'A complete log of this run can be found in:',
        ...npm.logFiles.map(f => '    ' + f),
      ].join('\n')
    )
  }

  // This removes any listeners npm setup and writes files if necessary
  // This is mostly used for tests to avoid max listener warnings
  if (hasLoadedNpm) {
    npm.unload()
  }

  // these are needed for the tests to have a clean slate in each test case
  exitHandlerCalled = false
  showLogFileMessage = false
})

const exitHandler = err => {
  exitHandlerCalled = true

  log.disableProgress()

  const hasNpm = !!npm
  const hasLoadedNpm = hasNpm && npm.config.loaded

  if (!hasNpm) {
    err = err || new Error('Exit prior to setting npm in exit handler')
    console.error(err.stack || err.message)
    return process.exit(1)
  }

  if (!hasLoadedNpm) {
    err = err || new Error('Exit prior to config file resolving.')
    console.error(err.stack || err.message)
  }

  // only show the notification if it finished.
  if (typeof npm.updateNotification === 'string') {
    const { level } = log
    log.level = 'notice'
    log.notice('', npm.updateNotification)
    log.level = level
  }

  let exitCode
  let noLogMessage

  if (err) {
    exitCode = 1
    // if we got a command that just shells out to something else, then it
    // will presumably print its own errors and exit with a proper status
    // code if there's a problem.  If we got an error with a code=0, then...
    // something else went wrong along the way, so maybe an npm problem?
    const isShellout = npm.shelloutCommands.includes(npm.command)
    const quietShellout = isShellout && typeof err.code === 'number' && err.code
    if (quietShellout) {
      exitCode = err.code
      noLogMessage = true
    } else if (typeof err === 'string') {
      // XXX: we should stop throwing strings
      log.error('', err)
      noLogMessage = true
    } else if (!(err instanceof Error)) {
      log.error('weird error', err)
      noLogMessage = true
    } else {
      if (!err.code) {
        const matchErrorCode = err.message.match(/^(?:Error: )?(E[A-Z]+)/)
        err.code = matchErrorCode && matchErrorCode[1]
      }

      for (const k of ['type', 'stack', 'statusCode', 'pkgid']) {
        const v = err[k]
        if (v) {
          log.verbose(k, replaceInfo(v))
        }
      }

      const args = replaceInfo(process.argv)
      log.verbose('cwd', process.cwd())
      log.verbose('', os.type() + ' ' + os.release())
      log.verbose('argv', args.map(JSON.stringify).join(' '))
      log.verbose('node', process.version)
      log.verbose('npm ', 'v' + npm.version)

      for (const k of ['code', 'syscall', 'file', 'path', 'dest', 'errno']) {
        const v = err[k]
        if (v) {
          log.error(k, v)
        }
      }

      const msg = errorMessage(err, npm)
      for (const errline of [...msg.summary, ...msg.detail]) {
        log.error(...errline)
      }

      if (hasLoadedNpm && npm.config.get('json')) {
        const error = {
          error: {
            code: err.code,
            summary: messageText(msg.summary),
            detail: messageText(msg.detail),
          },
        }
        console.error(JSON.stringify(error, null, 2))
      }

      if (typeof err.errno === 'number') {
        exitCode = err.errno
      } else if (typeof err.code === 'number') {
        exitCode = err.code
      }
    }
  }

  log.verbose('exit', exitCode || 0)

  showLogFileMessage = log.level === 'silent' || noLogMessage
    ? false
    : !!exitCode

  // explicitly call process.exit now so we don't hang on things like the
  // update notifier, also flush stdout beforehand because process.exit doesn't
  // wait for that to happen.
  process.stdout.write('', () => process.exit(exitCode))
}

module.exports = exitHandler
module.exports.setNpm = n => {
  npm = n
}
