var onSignalExit = require('../../')

onSignalExit(function (code, signal) {
  console.log('exited with sigterm, ' + code + ', ' + signal)
})

setTimeout(function () {}, 1000)

process.kill(process.pid, 'SIGTERM')
