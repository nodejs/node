
module.exports = errorHandler

var cbCalled = false
var log = require('npmlog')
var npm = require('../npm.js')
var rm = require('rimraf')
var itWorked = false
var path = require('path')
var wroteLogFile = false
var exitCode = 0
var rollbacks = npm.rollbacks
var chain = require('slide').chain
var writeStreamAtomic = require('fs-write-stream-atomic')
var errorMessage = require('./error-message.js')

process.on('exit', function (code) {
  log.disableProgress()
  if (!npm.config || !npm.config.loaded) return
  if (code) itWorked = false
  if (itWorked) log.info('ok')
  else {
    if (!cbCalled) {
      log.error('', 'cb() never called!')
    }

    if (wroteLogFile) {
      // just a line break
      if (log.levels[log.level] <= log.levels.error) console.error('')

      log.error(
        '',
        [
          'Please include the following file with any support request:',
          '    ' + path.resolve('npm-debug.log')
        ].join('\n')
      )
      wroteLogFile = false
    }
    if (code) {
      log.verbose('code', code)
    }
  }

  var doExit = npm.config.get('_exit')
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

  var doExit = npm.config ? npm.config.get('_exit') : true
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
        if (!code) errorHandler(er)
        else if (noLog) rm('npm-debug.log', reallyExit.bind(null, er))
        else writeLogFile(reallyExit.bind(this, er))
      } else {
        if (!noLog && code) writeLogFile(reallyExit)
        else rm('npm-debug.log', reallyExit)
      }
    })
    rollbacks.length = 0
  } else if (code && !noLog) {
    writeLogFile(reallyExit)
  } else {
    rm('npm-debug.log', reallyExit)
  }

  function reallyExit (er) {
    if (er && !code) code = typeof er.errno === 'number' ? er.errno : 1

    // truncate once it's been written.
    log.record.length = 0

    itWorked = !code

    // just emit a fake exit event.
    // if we're really exiting, then let it exit on its own, so that
    // in-process stuff can finish or clean up first.
    if (!doExit) process.emit('exit', code)
  }
}

function errorHandler (er) {
  log.disableProgress()
  // console.error('errorHandler', er)
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
    'fstream_path',
    'fstream_unc_path',
    'fstream_type',
    'fstream_class',
    'fstream_finish_call',
    'fstream_linkpath',
    'stack',
    'fstream_stack',
    'statusCode',
    'pkgid'
  ].forEach(function (k) {
    var v = er[k]
    if (!v) return
    if (k === 'fstream_stack') v = v.join('\n')
    log.verbose(k, v)
  })

  log.verbose('cwd', process.cwd())

  var os = require('os')
  // log.error('System', os.type() + ' ' + os.release())
  // log.error('command', process.argv.map(JSON.stringify).join(' '))
  // log.error('node -v', process.version)
  // log.error('npm -v', npm.version)
  log.error('', os.type() + ' ' + os.release())
  log.error('argv', process.argv.map(JSON.stringify).join(' '))
  log.error('node', process.version)
  log.error('npm ', 'v' + npm.version)

  ;[
    'file',
    'path',
    'code',
    'errno',
    'syscall'
  ].forEach(function (k) {
    var v = er[k]
    if (v) log.error(k, v)
  })

  // just a line break
  if (log.levels[log.level] <= log.levels.error) console.error('')

  var msg = errorMessage(er)
  msg.summary.concat(msg.detail).forEach(function (errline) {
    log.error.apply(log, errline)
  })

  exit(typeof er.errno === 'number' ? er.errno : 1)
}

var writingLogFile = false
function writeLogFile (cb) {
  if (writingLogFile) return cb()
  writingLogFile = true
  wroteLogFile = true

  var fstr = writeStreamAtomic('npm-debug.log')
  var os = require('os')
  var out = ''

  log.record.forEach(function (m) {
    var pref = [m.id, m.level]
    if (m.prefix) pref.push(m.prefix)
    pref = pref.join(' ')

    m.message.trim().split(/\r?\n/).map(function (line) {
      return (pref + ' ' + line).trim()
    }).forEach(function (line) {
      out += line + os.EOL
    })
  })

  fstr.end(out)
  fstr.on('close', cb)
}
