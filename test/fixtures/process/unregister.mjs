import { strictEqual } from 'assert'

function setup() {
  const obj = { foo: 'bar' }
  process.registerFreeOnExit(obj, shutdown)
  setImmediate(function () {
    process.unregisterFree(obj)
    process.unregisterFree(obj) // twice, this should not throw
  })
}

let shutdownCalled = false
function shutdown(obj) {
  shutdownCalled = true
}

setup()

process.on('exit', function () {
  strictEqual(shutdownCalled, false)
})