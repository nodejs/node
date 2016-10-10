var onSignalExit = require('../../')

onSignalExit(function (code, signal) {
  console.log('reached end of execution, ' + code + ', ' + signal)
})
