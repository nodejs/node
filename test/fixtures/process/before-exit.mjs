import { strictEqual } from 'assert'

let obj

function setup() {
  obj = { foo: 'bar' }
  process.finalization.registerBeforeExit(obj, shutdown)
}

let shutdownCalled = false
let timeoutFinished = false

function shutdown(obj, event) {
  shutdownCalled = true
  if (event === 'beforeExit') {
    setTimeout(function () {
      timeoutFinished = true
      strictEqual(obj.foo, 'bar')
      process.finalization.unregister(obj)
    }, 100)
    process.on('beforeExit', function () {
      strictEqual(timeoutFinished, true)
    })
  } else {
    throw new Error(`different event, expected beforeExit but got ${event}`)
  }
}

setup()

process.on('exit', function () {
  strictEqual(obj.foo, 'bar')
  strictEqual(shutdownCalled, true)
})
