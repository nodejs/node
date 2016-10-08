var onSignalExit = require('../../')

setTimeout(function () {})

var calledListener = 0
onSignalExit(function (code, signal) {
  console.log('exited calledListener=%j, code=%j, signal=%j',
              calledListener, code, signal)
})

process.on('SIGHUP', listener)
process.kill(process.pid, 'SIGHUP')

function listener () {
  calledListener++
  if (calledListener > 3) {
    process.removeListener('SIGHUP', listener)
  }

  setTimeout(function () {
    process.kill(process.pid, 'SIGHUP')
  })
}
