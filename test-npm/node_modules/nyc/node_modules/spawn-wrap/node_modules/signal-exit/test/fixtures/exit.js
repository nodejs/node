var onSignalExit = require('../../')

onSignalExit(function (code, signal) {
  console.log('exited with process.exit(), ' + code + ', ' + signal)
})

process.exit(32)
