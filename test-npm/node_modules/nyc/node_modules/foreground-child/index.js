var signalExit = require('signal-exit')
var spawn = require('child_process').spawn
if (process.platform === 'win32') {
  spawn = require('cross-spawn')
}

module.exports = function (program, args, cb) {
  var arrayIndex = arguments.length

  if (typeof args === 'function') {
    cb = args
    args = undefined
  } else {
    cb = Array.prototype.slice.call(arguments).filter(function (arg, i) {
      if (typeof arg === 'function') {
        arrayIndex = i
        return true
      }
    })[0]
  }

  cb = cb || function (done) {
    return done()
  }

  if (Array.isArray(program)) {
    args = program.slice(1)
    program = program[0]
  } else if (!Array.isArray(args)) {
    args = [].slice.call(arguments, 1, arrayIndex)
  }

  var spawnOpts = { stdio: [0, 1, 2] }

  if (process.send) {
    spawnOpts.stdio.push('ipc')
  }

  var child = spawn(program, args, spawnOpts)

  var childExited = false
  signalExit(function (code, signal) {
    child.kill(signal || 'SIGHUP')
  })

  child.on('close', function (code, signal) {
    // Allow the callback to inspect the child’s exit code and/or modify it.
    process.exitCode = signal ? 128 + signal : code

    cb(function () {
      childExited = true
      if (signal) {
        // If there is nothing else keeping the event loop alive,
        // then there's a race between a graceful exit and getting
        // the signal to this process.  Put this timeout here to
        // make sure we're still alive to get the signal, and thus
        // exit with the intended signal code.
        setTimeout(function () {}, 200)
        process.kill(process.pid, signal)
      } else {
        // Equivalent to process.exit() on Node.js >= 0.11.8
        process.exit(process.exitCode)
      }
    })
  })

  if (process.send) {
    process.removeAllListeners('message')

    child.on('message', function (message, sendHandle) {
      process.send(message, sendHandle)
    })

    process.on('message', function (message, sendHandle) {
      child.send(message, sendHandle)
    })
  }

  return child
}
