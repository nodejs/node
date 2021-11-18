const os = require('os')
const path = require('path')
const writeFileAtomic = require('write-file-atomic')
const mkdirp = require('mkdirp-infer-owner')
const fs = require('graceful-fs')

const errorMessage = require('./error-message.js')
const replaceInfo = require('./replace-info.js')

let exitHandlerCalled = false
let logFileName
let npm // set by the cli
let wroteLogFile = false

const getLogFile = () => {
  // we call this multiple times, so we need to treat it as a singleton because
  // the date is part of the name
  if (!logFileName) {
    logFileName = path.resolve(
      npm.config.get('cache'),
      '_logs',
      new Date().toISOString().replace(/[.:]/g, '_') + '-debug.log'
    )
  }

  return logFileName
}

process.on('exit', code => {
  // process.emit is synchronous, so the timeEnd handler will run before the
  // unfinished timer check below
  process.emit('timeEnd', 'npm')
  npm.log.disableProgress()
  for (const [name, timers] of npm.timers) {
    npm.log.verbose('unfinished npm timer', name, timers)
  }

  if (npm.config.loaded && npm.config.get('timing')) {
    try {
      const file = path.resolve(npm.config.get('cache'), '_timing.json')
      const dir = path.dirname(npm.config.get('cache'))
      mkdirp.sync(dir)

      fs.appendFileSync(
        file,
        JSON.stringify({
          command: process.argv.slice(2),
          logfile: getLogFile(),
          version: npm.version,
          ...npm.timings,
        }) + '\n'
      )

      const st = fs.lstatSync(path.dirname(npm.config.get('cache')))
      fs.chownSync(dir, st.uid, st.gid)
      fs.chownSync(file, st.uid, st.gid)
    } catch (ex) {
      // ignore
    }
  }

  if (!code) {
    npm.log.info('ok')
  } else {
    npm.log.verbose('code', code)
  }

  if (!exitHandlerCalled) {
    process.exitCode = code || 1
    npm.log.error('', 'Exit handler never called!')
    console.error('')
    npm.log.error('', 'This is an error with npm itself. Please report this error at:')
    npm.log.error('', '    <https://github.com/npm/cli/issues>')
    // TODO this doesn't have an npm.config.loaded guard
    writeLogFile()
  }
  // In timing mode we always write the log file
  if (npm.config.loaded && npm.config.get('timing') && !wroteLogFile) {
    writeLogFile()
  }
  if (wroteLogFile) {
    // just a line break
    if (npm.log.levels[npm.log.level] <= npm.log.levels.error) {
      console.error('')
    }

    npm.log.error(
      '',
      ['A complete log of this run can be found in:', '    ' + getLogFile()].join('\n')
    )
  }

  // these are needed for the tests to have a clean slate in each test case
  exitHandlerCalled = false
  wroteLogFile = false
})

const exitHandler = err => {
  npm.log.disableProgress()
  if (!npm.config.loaded) {
    err = err || new Error('Exit prior to config file resolving.')
    console.error(err.stack || err.message)
  }

  // only show the notification if it finished.
  if (typeof npm.updateNotification === 'string') {
    const { level } = npm.log
    npm.log.level = 'notice'
    npm.log.notice('', npm.updateNotification)
    npm.log.level = level
  }

  exitHandlerCalled = true

  let exitCode
  let noLog

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
      noLog = true
    } else if (typeof err === 'string') {
      noLog = true
      npm.log.error('', err)
    } else if (!(err instanceof Error)) {
      noLog = true
      npm.log.error('weird error', err)
    } else {
      if (!err.code) {
        const matchErrorCode = err.message.match(/^(?:Error: )?(E[A-Z]+)/)
        err.code = matchErrorCode && matchErrorCode[1]
      }

      for (const k of ['type', 'stack', 'statusCode', 'pkgid']) {
        const v = err[k]
        if (v) {
          npm.log.verbose(k, replaceInfo(v))
        }
      }

      npm.log.verbose('cwd', process.cwd())

      const args = replaceInfo(process.argv)
      npm.log.verbose('', os.type() + ' ' + os.release())
      npm.log.verbose('argv', args.map(JSON.stringify).join(' '))
      npm.log.verbose('node', process.version)
      npm.log.verbose('npm ', 'v' + npm.version)

      for (const k of ['code', 'syscall', 'file', 'path', 'dest', 'errno']) {
        const v = err[k]
        if (v) {
          npm.log.error(k, v)
        }
      }

      const msg = errorMessage(err, npm)
      for (const errline of [...msg.summary, ...msg.detail]) {
        npm.log.error(...errline)
      }

      if (npm.config.loaded && npm.config.get('json')) {
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
  npm.log.verbose('exit', exitCode || 0)

  if (npm.log.level === 'silent') {
    noLog = true
  }

  // noLog is true if there was an error, including if config wasn't loaded, so
  // this doesn't need a config.loaded guard
  if (exitCode && !noLog) {
    writeLogFile()
  }

  // explicitly call process.exit now so we don't hang on things like the
  // update notifier, also flush stdout beforehand because process.exit doesn't
  // wait for that to happen.
  process.stdout.write('', () => process.exit(exitCode))
}

const messageText = msg => msg.map(line => line.slice(1).join(' ')).join('\n')

const writeLogFile = () => {
  try {
    let logOutput = ''
    npm.log.record.forEach(m => {
      const p = [m.id, m.level]
      if (m.prefix) {
        p.push(m.prefix)
      }
      const pref = p.join(' ')

      m.message
        .trim()
        .split(/\r?\n/)
        .map(line => (pref + ' ' + line).trim())
        .forEach(line => {
          logOutput += line + os.EOL
        })
    })

    const file = getLogFile()
    const dir = path.dirname(file)
    mkdirp.sync(dir)
    writeFileAtomic.sync(file, logOutput)

    const st = fs.lstatSync(path.dirname(npm.config.get('cache')))
    fs.chownSync(dir, st.uid, st.gid)
    fs.chownSync(file, st.uid, st.gid)

    // truncate once it's been written.
    npm.log.record.length = 0
    wroteLogFile = true
  } catch (ex) {}
}

module.exports = exitHandler
module.exports.setNpm = n => {
  npm = n
}
