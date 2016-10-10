var onSignalExit = require('../..')
onSignalExit(function (code, signal) {
  console.error('onSignalExit(%j,%j)', code, signal)
})
setTimeout(function () {
  console.log('hello')
})
process.kill(process.pid, 'SIGPIPE')
