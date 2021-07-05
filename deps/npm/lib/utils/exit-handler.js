const log = require('npmlog')
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

const timings = {}

const getLogFile = () => {
  // we call this multiple times, so we need to treat it as a singleton because
  // the date is part of the name
  if (!logFileName)
    logFileName = path.resolve(npm.config.get('cache'), '_logs', (new Date()).toISOString().replace(/[.:]/g, '_') + '-debug.log')

  return logFileName
}

process.on('timing', (name, value) => {
  if (timings[name])
    timings[name] += value
  else
    timings[name] = value
})

process.on('exit', code => {
  process.emit('timeEnd', 'npm')
  log.disableProgress()
  if (npm.config && npm.config.loaded && npm.config.get('timing')) {
    try {
      const file = path.resolve(npm.config.get('cache'), '_timing.json')
      const dir = path.dirname(npm.config.get('cache'))
      mkdirp.sync(dir)

      fs.appendFileSync(file, JSON.stringify({
        command: process.argv.slice(2),
        logfile: getLogFile(),
        version: npm.version,
        ...timings,
      }) + '\n')

      const st = fs.lstatSync(path.dirname(npm.config.get('cache')))
      fs.chownSync(dir, st.uid, st.gid)
      fs.chownSync(file, st.uid, st.gid)
    } catch (ex) {
      // ignore
    }
  }

  if (!code)
    log.info('ok')
  else {
    log.verbose('code', code)
    if (!exitHandlerCalled) {
      log.error('', 'Exit handler never called!')
      console.error('')
      log.error('', 'This is an error with npm itself. Please report this error at:')
      log.error('', '    <https://github.com/npm/cli/issues>')
      // TODO this doesn't have an npm.config.loaded guard
      writeLogFile()
    }
  }
  // In timing mode we always write the log file
  if (npm.config && npm.config.loaded && npm.config.get('timing') && !wroteLogFile)
    writeLogFile()
  if (wroteLogFile) {
    // just a line break
    if (log.levels[log.level] <= log.levels.error)
      console.error('')

    log.error(
      '',
      [
        'A complete log of this run can be found in:',
        '    ' + getLogFile(),
      ].join('\n')
    )
  }

  // these are needed for the tests to have a clean slate in each test case
  exitHandlerCalled = false
  wroteLogFile = false

  // actually exit.
  process.exit(code)
})

const exit = (code, noLog) => {
  log.verbose('exit', code || 0)
  if (log.level === 'silent')
    noLog = true

  // noLog is true if there was an error, including if config wasn't loaded, so
  // this doesn't need a config.loaded guard
  if (code && !noLog)
    writeLogFile()

  // Exit directly -- nothing in the CLI should still be running in the
  // background at this point, and this makes sure anything left dangling
  // for whatever reason gets thrown away, instead of leaving the CLI open
  process.stdout.write('', () => {
    process.exit(code)
  })
}

const exitHandler = (err) => {
  log.disableProgress()
  if (!npm.config || !npm.config.loaded) {
    // logging won't work unless we pretend that it's ready
    err = err || new Error('Exit prior to config file resolving.')
    console.error(err.stack || err.message)
  }

  if (exitHandlerCalled)
    err = err || new Error('Exit handler called more than once.')

  // only show the notification if it finished before the other stuff we
  // were doing.  no need to hang on `npm -v` or something.
  if (typeof npm.updateNotification === 'string') {
    const { level } = log
    log.level = log.levels.notice
    log.notice('', npm.updateNotification)
    log.level = level
  }

  exitHandlerCalled = true
  if (!err)
    return exit()

  // if we got a command that just shells out to something else, then it
  // will presumably print its own errors and exit with a proper status
  // code if there's a problem.  If we got an error with a code=0, then...
  // something else went wrong along the way, so maybe an npm problem?
  const isShellout = npm.shelloutCommands.includes(npm.command)
  const quietShellout = isShellout && typeof err.code === 'number' && err.code
  if (quietShellout)
    return exit(err.code, true)
  else if (typeof err === 'string') {
    log.error('', err)
    return exit(1, true)
  } else if (!(err instanceof Error)) {
    log.error('weird error', err)
    return exit(1, true)
  }

  if (!err.code) {
    const matchErrorCode = err.message.match(/^(?:Error: )?(E[A-Z]+)/)
    err.code = matchErrorCode && matchErrorCode[1]
  }

  for (const k of ['type', 'stack', 'statusCode', 'pkgid']) {
    const v = err[k]
    if (v)
      log.verbose(k, replaceInfo(v))
  }

  log.verbose('cwd', process.cwd())

  const args = replaceInfo(process.argv)
  log.verbose('', os.type() + ' ' + os.release())
  log.verbose('argv', args.map(JSON.stringify).join(' '))
  log.verbose('node', process.version)
  log.verbose('npm ', 'v' + npm.version)

  for (const k of ['code', 'syscall', 'file', 'path', 'dest', 'errno']) {
    const v = err[k]
    if (v)
      log.error(k, v)
  }

  const msg = errorMessage(err, npm)
  for (const errline of [...msg.summary, ...msg.detail])
    log.error(...errline)

  if (npm.config && npm.config.get('json')) {
    const error = {
      error: {
        code: err.code,
        summary: messageText(msg.summary),
        detail: messageText(msg.detail),
      },
    }
    console.error(JSON.stringify(error, null, 2))
  }

  exit(typeof err.errno === 'number' ? err.errno : typeof err.code === 'number' ? err.code : 1)
}

const messageText = msg => msg.map(line => line.slice(1).join(' ')).join('\n')

const writeLogFile = () => {
  if (wroteLogFile)
    return

  try {
    let logOutput = ''
    log.record.forEach(m => {
      const p = [m.id, m.level]
      if (m.prefix)
        p.push(m.prefix)
      const pref = p.join(' ')

      m.message.trim().split(/\r?\n/)
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
    log.record.length = 0
    wroteLogFile = true
  } catch (ex) {

  }
}

module.exports = exitHandler
module.exports.setNpm = (n) => {
  npm = n
}
