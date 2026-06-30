import { deepStrictEqual } from 'assert'
import { setImmediate } from 'timers/promises'

const keptAlive = []
const finalized = []

function onFinalize(obj) {
  finalized.push(obj.name)
}

{
  const first = { name: 'first' }
  let collected = { name: 'collected' }
  const third = { name: 'third' }

  keptAlive.push(first, third)

  process.finalization.register(first, onFinalize)
  process.finalization.register(collected, onFinalize)
  process.finalization.register(third, onFinalize)

  collected = null
}

// Give V8 a few chances to collect `collected` and run the
// FinalizationRegistry cleanup before process exit.
for (let i = 0; i < 10; i++) {
  gc()
  await setImmediate()
}

process.on('exit', function () {
  deepStrictEqual(finalized, ['first', 'third'])
})
