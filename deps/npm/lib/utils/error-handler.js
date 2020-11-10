
module.exports = errorHandler
module.exports.exit = exit

var cbCalled = false
var log = require('npmlog')
var npm = require('../npm.js')
var itWorked = false
var path = require('path')
var wroteLogFile = false
var exitCode = 0
var rollbacks = npm.rollbacks
var chain = require('slide').chain
var errorMessage = require('./error-message.js')
var replaceInfo = require('./replace-info.js')
var stopMetrics = require('./metrics.js').stop

const cacheFile = require('./cache-file.js')

var logFileName
function getLogFile () {
  if (!logFileName) {
    logFileName = path.resolve(npm.config.get('cache'), '_logs', (new Date()).toISOString().replace(/[.:]/g, '_') + '-debug.log')
  }
  return logFileName
}

var timings = {
  version: npm.version,
  command: process.argv.slice(2),
  logfile: null
}
process.on('timing', function (name, value) {
  if (timings[name]) { timings[name] += value } else { timings[name] = value }
})

process.on('exit', function (code) {
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

  // kill any outstanding stats reporter if it hasn't finished yet
  stopMetrics()

  if (code) itWorked = false
  if (itWorked) {
    log.info('ok')
  } else {
    if (!cbCalled) {
      log.error('', 'cb() never called!')
      console.error('')
      log.error('', 'This is an error with npm itself. Please report this error at:')
      log.error('', '    <https://npm.community>')
      writeLogFile()
    }

    if (code) {
      log.verbose('code', code)
    }
  }
  if (npm.config && npm.config.loaded && npm.config.get('timing') && !wroteLogFile) writeLogFile()
  if (wroteLogFile) {
    // just a line break
    if (log.levels[log.level] <= log.levels.error) console.error('')

    log.error(
      '',
      [
        'A complete log of this run can be found in:',
        '    ' + getLogFile()
      ].join('\n')
    )
    wroteLogFile = false
  }

  var doExit = npm.config && npm.config.loaded && npm.config.get('_exit')
  if (doExit) {
    // actually exit.
    if (exitCode === 0 && !itWorked) {
      exitCode = 1
    }
    if (exitCode !== 0) process.exit(exitCode)
  } else {
    itWorked = false // ready for next exit
  }
})

function exit (code, noLog) {
  exitCode = exitCode || process.exitCode || code

  var doExit = npm.config && npm.config.loaded ? npm.config.get('_exit') : true
  log.verbose('exit', [code, doExit])
  if (log.level === 'silent') noLog = true

  if (rollbacks.length) {
    chain(rollbacks.map(function (f) {
      return function (cb) {
        npm.commands.unbuild([f], true, cb)
      }
    }), function (er) {
      if (er) {
        log.error('error rolling back', er)
        if (!code) {
          errorHandler(er)
        } else {
          if (!noLog) writeLogFile()
          reallyExit(er)
        }
      } else {
        if (!noLog && code) writeLogFile()
        reallyExit()
      }
    })
    rollbacks.length = 0
  } else if (code && !noLog) {
    writeLogFile()
  } else {
    reallyExit()
  }

  function reallyExit (er) {
    if (er && !code) code = typeof er.errno === 'number' ? er.errno : 1

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
}

function errorHandler (er) {
  log.disableProgress()
  if (!npm.config || !npm.config.loaded) {
    // logging won't work unless we pretend that it's ready
    er = er || new Error('Exit prior to config file resolving.')
    console.error(er.stack || er.message)
  }

  if (cbCalled) {
    er = er || new Error('Callback called more than once.')
  }

  cbCalled = true
  if (!er) return exit(0)
  if (typeof er === 'string') {
    log.error('', er)
    return exit(1, true)
  } else if (!(er instanceof Error)) {
    log.error('weird error', er)
    return exit(1, true)
  }

  var m = er.code || er.message.match(/^(?:Error: )?(E[A-Z]+)/)
  if (m && !er.code) {
    er.code = m
  }

  ;[
    'type',
    'stack',
    'statusCode',
    'pkgid'
  ].forEach(function (k) {
    var v = er[k]
    if (!v) return
    v = replaceInfo(v)
    log.verbose(k, v)
  })

  log.verbose('cwd', process.cwd())

  var os = require('os')
  var args = replaceInfo(process.argv)
  log.verbose('', os.type() + ' ' + os.release())
  log.verbose('argv', args.map(JSON.stringify).join(' '))
  log.verbose('node', process.version)
  log.verbose('npm ', 'v' + npm.version)

  ;[
    'code',
    'syscall',
    'file',
    'path',
    'dest',
    'errno'
  ].forEach(function (k) {
    var v = er[k]
    if (v) log.error(k, v)
  })

  var msg = errorMessage(er)
  msg.summary.concat(msg.detail).forEach(function (errline) {
    log.error.apply(log, errline)
  })
  if (npm.config && npm.config.get('json')) {
    var error = {
      error: {
        code: er.code,
        summary: messageText(msg.summary),
        detail: messageText(msg.detail)
      }
    }
    console.log(JSON.stringify(error, null, 2))
  }

  exit(typeof er.errno === 'number' ? er.errno : 1)
}

function messageText (msg) {
  return msg.map(function (line) {
    return line.slice(1).join(' ')
  }).join('\n')
}

function writeLogFile () {
  if (wroteLogFile) return

  var os = require('os')

  try {
    var logOutput = ''
    log.record.forEach(function (m) {
      var pref = [m.id, m.level]
      if (m.prefix) pref.push(m.prefix)
      pref = pref.join(' ')

      m.message.trim().split(/\r?\n/).map(function (line) {
        return (pref + ' ' + line).trim()
      }).forEach(function (line) {
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
