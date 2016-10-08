if (process.argv.length === 2) {
  var types = [ 'explicit', 'code', 'normal' ]
  var codes = [ 0, 2, 'null' ]
  var changes = [ 'nochange', 'change', 'code', 'twice', 'twicecode']
  var handlers = [ 'sigexit', 'nosigexit' ]
  var opts = []
  types.forEach(function (type) {
    var testCodes = type === 'normal' ? [ 0 ] : codes
    testCodes.forEach(function (code) {
      changes.forEach(function (change) {
        handlers.forEach(function (handler) {
          opts.push([type, code, change, handler].join(' '))
        })
      })
    })
  })

  var results = {}

  var exec = require('child_process').exec
  run(opts.shift())
} else {
  var type = process.argv[2]
  var code = +process.argv[3]
  var change = process.argv[4]
  var sigexit = process.argv[5] !== 'nosigexit'

  if (sigexit) {
    var onSignalExit = require('../../')
    onSignalExit(listener)
  } else {
    process.on('exit', listener)
  }

  process.on('exit', function (code) {
    console.error('first code=%j', code)
  })

  if (change !== 'nochange') {
    process.once('exit', function (code) {
      console.error('set code from %j to %j', code, 5)
      if (change === 'code' || change === 'twicecode') {
        process.exitCode = 5
      } else {
        process.exit(5)
      }
    })
    if (change === 'twicecode' || change === 'twice') {
      process.once('exit', function (code) {
        code = process.exitCode || code
        console.error('set code from %j to %j', code, code + 1)
        process.exit(code + 1)
      })
    }
  }

  process.on('exit', function (code) {
    console.error('second code=%j', code)
  })

  if (type === 'explicit') {
    if (code || code === 0) {
      process.exit(code)
    } else {
      process.exit()
    }
  } else if (type === 'code') {
    process.exitCode = +code || 0
  }
}

function listener (code, signal) {
  signal = signal || null
  console.log('%j', { code: code, signal: signal, exitCode: process.exitCode || 0 })
}

function run (opt) {
  console.error(opt)
  exec(process.execPath + ' ' + __filename + ' ' + opt, function (err, stdout, stderr) {
    var res = JSON.parse(stdout)
    if (err) {
      res.actualCode = err.code
      res.actualSignal = err.signal
    } else {
      res.actualCode = 0
      res.actualSignal = null
    }
    res.stderr = stderr.trim().split('\n')
    results[opt] = res
    if (opts.length) {
      run(opts.shift())
    } else {
      console.log(JSON.stringify(results, null, 2))
    }
  })
}
