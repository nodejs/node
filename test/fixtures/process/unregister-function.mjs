import { strictEqual } from 'assert'

const calls = []

function onExit(obj, event) {
  calls.push(`${obj.name}:${event}`)
}

const a = { name: 'a' }
const b = { name: 'b' }

const unregisterA = process.finalization.register(a, onExit)
const unregisterABeforeExit = process.finalization.registerBeforeExit(a, onExit)
process.finalization.register(b, onExit)
const unregisterBAgain = process.finalization.register(b, onExit)

strictEqual(typeof unregisterA, 'function')
strictEqual(typeof unregisterABeforeExit, 'function')

unregisterA()
unregisterA() // twice, this should not throw
unregisterABeforeExit()

// Removing one registration keeps the others for the same object.
unregisterBAgain()

process.on('exit', function () {
  strictEqual(calls.join(','), 'b:exit')
})
