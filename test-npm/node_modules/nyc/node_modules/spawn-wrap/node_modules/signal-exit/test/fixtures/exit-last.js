var onSignalExit = require('../../')
var counter = 0

onSignalExit(function (code, signal) {
  counter++
  console.log('last counter=%j, code=%j, signal=%j',
              counter, code, signal)
}, {alwaysLast: true})

onSignalExit(function (code, signal) {
  counter++
  console.log('first counter=%j, code=%j, signal=%j',
              counter, code, signal)
})
