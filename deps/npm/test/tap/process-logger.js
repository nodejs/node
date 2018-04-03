'use strict'
const test = require('tap').test
require('../../lib/npm.js')

test('process logging', (t) => {
  t.ok(process.listenerCount('log') >= 1, `log listener attached ${process.listenerCount('log')} >= 1`)
  t.doesNotThrow(() => process.emit('log', 'error', 'test', 'this'), 'logging does not throw')
  t.doesNotThrow(() => process.emit('log', 2348), 'invalid args do not throw')
  t.doesNotThrow(() => process.emit('log', null), 'null does not throw')
  t.doesNotThrow(() => process.emit('log', {}), 'obj does not throw')
  t.done()
})
