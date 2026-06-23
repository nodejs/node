import { strictEqual } from 'assert'

let obj

function setup() {
  obj = { foo: 'bar' }
  process.finalization.register(obj, shutdown)
}

let shutdownCalled = false
function shutdown(obj) {
  shutdownCalled = true
  strictEqual(obj.foo, 'bar')
}

setup()

process.on('exit', function () {
  strictEqual(obj.foo, 'bar')
  strictEqual(shutdownCalled, true)
})
