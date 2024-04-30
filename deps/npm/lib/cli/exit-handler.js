const { log, output, META } = require('proc-log')
const errorMessage = require('../utils/error-message.js')
const { redactLog: replaceInfo } = require('@npmcli/redact')

let npm = null // set by the cli
let exitHandlerCalled = false
let showLogFileError = false

process.on('exit', code => {
  const hasLoadedNpm = npm?.config.loaded

  if (!code) {
    log.info('ok')
  } else {
    log.verbose('code', code)
  }

  if (!exitHandlerCalled) {
    process.exitCode = code || 1
    log.error('', 'Exit handler never called!')
    log.error('', 'This is an error with npm itself. Please report this error at:')
    log.error('', '    <https://github.com/npm/cli/issues>')
    // eslint-disable-next-line no-console
    console.error('')
    showLogFileError = true
  }

  // npm must be loaded to know where the log file was written
  if (hasLoadedNpm) {
    npm.finish({ showLogFileError })
    // This removes any listeners npm setup, mostly for tests to avoid max listener warnings
    npm.unload()
  }

  // these are needed for the tests to have a clean slate in each test case
  exitHandlerCalled = false
  showLogFileError = false
})

const exitHandler = err => {
  exitHandlerCalled = true

  const hasLoadedNpm = npm?.config.loaded

  if (!npm) {
    err = err || new Error('Exit prior to setting npm in exit handler')
    // Don't use proc-log here since npm was never set
    // eslint-disable-next-line no-console
    console.error(err.stack || err.message)
    return process.exit(1)
  }

  if (!hasLoadedNpm) {
    err = err || new Error('Exit prior to config file resolving.')
    // Don't use proc-log here since npm was never loaded
    // eslint-disable-next-line no-console
    console.error(err.stack || err.message)
  }

  // only show the notification if it finished.
  if (typeof npm.updateNotification === 'string') {
    log.notice('', npm.updateNotification, { [META]: true, force: true })
  }

  let exitCode = process.exitCode || 0
  let noLogMessage = exitCode !== 0
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
      const os = require('node:os')
      const fs = require('node:fs')
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
    output.flush({ [META]: true, jsonError })
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
