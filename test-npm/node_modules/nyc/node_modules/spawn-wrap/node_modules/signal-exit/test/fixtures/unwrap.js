// simulate cases where the module could be loaded from multiple places

// Need to lie about this a little bit, since nyc uses this module
// for its coverage wrap-up handling
if (process.env.NYC_CWD) {
  var emitter = process.__signal_exit_emitter__
  var listeners = emitter.listeners('afterexit')
  process.removeAllListeners('SIGHUP')
  delete process.__signal_exit_emitter__
  delete require('module')._cache[require.resolve('../../')]
}

var onSignalExit = require('../../')
var counter = 0

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

if (global.__coverage__ && listeners && listeners.length) {
  listeners.forEach(function (fn) {
    onSignalExit(fn, { alwaysLast: true })
  })
}

process.kill(process.pid, 'SIGHUP')
setTimeout(function () {}, 1000)
