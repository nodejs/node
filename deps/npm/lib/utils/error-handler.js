let cbCalled = false
const log = require('npmlog')
const npm = require('../npm.js')
let itWorked = false
const path = require('path')
let wroteLogFile = false
let exitCode = 0
const errorMessage = require('./error-message.js')
const replaceInfo = require('./replace-info.js')

const cacheFile = require('./cache-file.js')

let logFileName
const getLogFile = () => {
  if (!logFileName)
    logFileName = path.resolve(npm.config.get('cache'), '_logs', (new Date()).toISOString().replace(/[.:]/g, '_') + '-debug.log')

  return logFileName
}

const timings = {
  version: npm.version,
  command: process.argv.slice(2),
  logfile: null,
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
      timings.logfile = getLogFile()
      cacheFile.append('_timing.json', JSON.stringify(timings) + '\n')
    } catch (_) {
      // ignore
    }
  }

  if (code)
    itWorked = false
  if (itWorked)
    log.info('ok')
  else {
    if (!cbCalled) {
      log.error('', 'cb() never called!')
      console.error('')
      log.error('', 'This is an error with npm itself. Please report this error at:')
      log.error('', '    <https://github.com/npm/cli/issues>')
      writeLogFile()
    }

    if (code)
      log.verbose('code', code)
  }
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
    wroteLogFile = false
  }

  // actually exit.
  if (exitCode === 0 && !itWorked)
    exitCode = 1

  if (exitCode !== 0)
    process.exit(exitCode)
})

const exit = (code, noLog) => {
  exitCode = exitCode || process.exitCode || code

  log.verbose('exit', code)
  if (log.level === 'silent')
    noLog = true

  const reallyExit = () => {
    itWorked = !code

    // Exit directly -- nothing in the CLI should still be running in the
    // background at this point, and this makes sure anything left dangling
    // for whatever reason gets thrown away, instead of leaving the CLI open
    //
    // Commands that expect long-running actions should just delay `cb()`
    process.stdout.write('', () => {
      process.exit(code)
    })
  }

  if (code && !noLog)
    writeLogFile()
  reallyExit()
}

const errorHandler = (er) => {
  log.disableProgress()
  if (!npm.config || !npm.config.loaded) {
    // logging won't work unless we pretend that it's ready
    er = er || new Error('Exit prior to config file resolving.')
    console.error(er.stack || er.message)
  }

  if (cbCalled)
    er = er || new Error('Callback called more than once.')

  if (npm.updateNotification) {
    const { level } = log
    log.level = log.levels.notice
    log.notice('', npm.updateNotification)
    log.level = level
  }

  cbCalled = true
  if (!er)
    return exit(0)

  // if we got a command that just shells out to something else, then it
  // will presumably print its own errors and exit with a proper status
  // code if there's a problem.  If we got an error with a code=0, then...
  // something else went wrong along the way, so maybe an npm problem?
  const isShellout = npm.shelloutCommands.includes(npm.command)
  const quietShellout = isShellout && typeof er.code === 'number' && er.code
  if (quietShellout)
    return exit(er.code, true)
  else if (typeof er === 'string') {
    log.error('', er)
    return exit(1, true)
  } else if (!(er instanceof Error)) {
    log.error('weird error', er)
    return exit(1, true)
  }

  if (!er.code) {
    const matchErrorCode = er.message.match(/^(?:Error: )?(E[A-Z]+)/)
    er.code = matchErrorCode && matchErrorCode[1]
  }

  for (const k of ['type', 'stack', 'statusCode', 'pkgid']) {
    const v = er[k]
    if (v)
      log.verbose(k, replaceInfo(v))
  }

  log.verbose('cwd', process.cwd())

  const os = require('os')
  const args = replaceInfo(process.argv)
  log.verbose('', os.type() + ' ' + os.release())
  log.verbose('argv', args.map(JSON.stringify).join(' '))
  log.verbose('node', process.version)
  log.verbose('npm ', 'v' + npm.version)

  for (const k of ['code', 'syscall', 'file', 'path', 'dest', 'errno']) {
    const v = er[k]
    if (v)
      log.error(k, v)
  }

  const msg = errorMessage(er)
  for (const errline of [...msg.summary, ...msg.detail])
    log.error(...errline)

  if (npm.config && npm.config.get('json')) {
    const error = {
      error: {
        code: er.code,
        summary: messageText(msg.summary),
        detail: messageText(msg.detail),
      },
    }
    console.error(JSON.stringify(error, null, 2))
  }

  exit(typeof er.errno === 'number' ? er.errno : typeof er.code === 'number' ? er.code : 1)
}

const messageText = msg => msg.map(line => line.slice(1).join(' ')).join('\n')

const writeLogFile = () => {
  if (wroteLogFile)
    return

  const os = require('os')

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
    cacheFile.write(getLogFile(), logOutput)

    // truncate once it's been written.
    log.record.length = 0
    wroteLogFile = true
  } catch (ex) {

  }
}

module.exports = errorHandler
module.exports.exit = exit
