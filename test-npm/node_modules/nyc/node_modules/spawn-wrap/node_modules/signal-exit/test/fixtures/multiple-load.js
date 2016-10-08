// simulate cases where the module could be loaded from multiple places
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

delete require('module')._cache[require.resolve('../../')]
var onSignalExit = require('../../')

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

// Lastly, some that should NOT be shown
delete require('module')._cache[require.resolve('../../')]
var onSignalExit = require('../../')

var unwrap = onSignalExit(function (code, signal) {
  counter++
  console.log('last counter=%j, code=%j, signal=%j',
              counter, code, signal)
}, {alwaysLast: true})
unwrap()

unwrap = onSignalExit(function (code, signal) {
  counter++
  console.log('first counter=%j, code=%j, signal=%j',
              counter, code, signal)
})

unwrap()

process.kill(process.pid, 'SIGHUP')
setTimeout(function () {}, 1000)
