const os = require('os')
const fs = require('fs')

const log = require('./log-shim.js')
const errorMessage = require('./error-message.js')
const replaceInfo = require('./replace-info.js')

let npm = null // set by the cli
let exitHandlerCalled = false
let showLogFileError = false

process.on('exit', code => {
  log.disableProgress()

  // process.emit is synchronous, so the timeEnd handler will run before the
  // unfinished timer check below
  process.emit('timeEnd', 'npm')

  const hasLoadedNpm = npm?.config.loaded

  // Unfinished timers can be read before config load
  if (npm) {
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
    // eslint-disable-next-line no-console
    console.error('')
    log.error('', 'This is an error with npm itself. Please report this error at:')
    log.error('', '    <https://github.com/npm/cli/issues>')
    showLogFileError = true
  }

  // npm must be loaded to know where the log file was written
  if (hasLoadedNpm) {
    // write the timing file now, this might do nothing based on the configs set.
    // we need to call it here in case it errors so we dont tell the user
    // about a timing file that doesn't exist
    npm.writeTimingFile()

    const logsDir = npm.logsDir
    const logFiles = npm.logFiles

    const timingDir = npm.timingDir
    const timingFile = npm.timingFile

    const timing = npm.config.get('timing')
    const logsMax = npm.config.get('logs-max')

    // Determine whether to show log file message and why it is
    // being shown since in timing mode we always show the log file message
    const logMethod = showLogFileError ? 'error' : timing ? 'info' : null

    if (logMethod) {
      if (!npm.silent) {
        // just a line break if not in silent mode
        // eslint-disable-next-line no-console
        console.error('')
      }

      const message = []

      if (timingFile) {
        message.push(`Timing info written to: ${timingFile}`)
      } else if (timing) {
        message.push(
          `The timing file was not written due to an error writing to the directory: ${timingDir}`
        )
      }

      if (logFiles.length) {
        message.push(`A complete log of this run can be found in: ${logFiles}`)
      } else if (logsMax <= 0) {
        // user specified no log file
        message.push(`Log files were not written due to the config logs-max=${logsMax}`)
      } else {
        // could be an error writing to the directory
        message.push(
          `Log files were not written due to an error writing to the directory: ${logsDir}`,
          'You can rerun the command with `--loglevel=verbose` to see the logs in your terminal'
        )
      }

      log[logMethod]('', message.join('\n'))
    }

    // This removes any listeners npm setup, mostly for tests to avoid max listener warnings
    npm.unload()
  }

  // these are needed for the tests to have a clean slate in each test case
  exitHandlerCalled = false
  showLogFileError = false
})

const exitHandler = err => {
  exitHandlerCalled = true

  log.disableProgress()

  const hasLoadedNpm = npm?.config.loaded

  if (!npm) {
    err = err || new Error('Exit prior to setting npm in exit handler')
    // eslint-disable-next-line no-console
    console.error(err.stack || err.message)
    return process.exit(1)
  }

  if (!hasLoadedNpm) {
    err = err || new Error('Exit prior to config file resolving.')
    // eslint-disable-next-line no-console
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
  let jsonError

  if (err) {
    exitCode = 1
    // if we got a command that just shells out to something else, then it
    // will presumably print its own errors and exit with a proper status
    // code if there's a problem.  If we got an error with a code=0, then...
    // something else went wrong along the way, so maybe an npm problem?
    const isShellout = npm.isShellout
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

      log.verbose('cwd', process.cwd())
      log.verbose('', os.type() + ' ' + os.release())
      log.verbose('node', process.version)
      log.verbose('npm ', 'v' + npm.version)

      for (const k of ['code', 'syscall', 'file', 'path', 'dest', 'errno']) {
        const v = err[k]
        if (v) {
          log.error(k, v)
        }
      }

      const { summary, detail, json, files = [] } = errorMessage(err, npm)
      jsonError = json

      for (let [file, content] of files) {
        file = `${npm.logPath}${file}`
        content = `'Log files:\n${npm.logFiles.join('\n')}\n\n${content.trim()}\n`
        try {
          fs.writeFileSync(file, content)
          detail.push(['', `\n\nFor a full report see:\n${file}`])
        } catch (logFileErr) {
          log.warn('', `Could not write error message to ${file} due to ${logFileErr}`)
        }
      }

      for (const errline of [...summary, ...detail]) {
        log.error(...errline)
      }

      if (typeof err.errno === 'number') {
        exitCode = err.errno
      } else if (typeof err.code === 'number') {
        exitCode = err.code
      }
    }
  }

  if (hasLoadedNpm) {
    npm.flushOutput(jsonError)
  }

  log.verbose('exit', exitCode || 0)

  showLogFileError = (hasLoadedNpm && npm.silent) || noLogMessage
    ? false
    : !!exitCode

  // explicitly call process.exit now so we don't hang on things like the
  // update notifier, also flush stdout/err beforehand because process.exit doesn't
  // wait for that to happen.
  let flushed = 0
  const flush = [process.stderr, process.stdout]
  const exit = () => ++flushed === flush.length && process.exit(exitCode)
  flush.forEach((f) => f.write('', exit))
}

module.exports = exitHandler
module.exports.setNpm = n => (npm = n)
