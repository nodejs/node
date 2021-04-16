const t = require('tap')

let pulseStarted = null
const npmlog = {
  gauge: {
    pulse: () => {
      if (pulseStarted)
        pulseStarted()
    },
  },
}

const pulseTillDone = t.mock('../../../lib/utils/pulse-till-done.js', {
  npmlog,
})

t.test('pulses (with promise)', async (t) => {
  t.teardown(() => {
    pulseStarted = null
  })

  let resolver
  const promise = new Promise(resolve => {
    resolver = resolve
  })

  const result = pulseTillDone.withPromise(promise)
  // wait until the gauge has fired at least once
  await new Promise(resolve => {
    pulseStarted = resolve
  })
  resolver('value')
  t.resolveMatch(result, 'value', 'returned the resolved promise')
})
