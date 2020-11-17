const t = require('tap')
const logs = []
const npmlog = require('npmlog')
npmlog.silly = (...msg) => logs.push(['silly', ...msg])
npmlog.timing = (...msg) => logs.push(['timing', ...msg])

t.test('time some stuff', t => {
  const timings = {}
  process.on('timing', (name, value) => {
    timings[name] = (timings[name] || 0) + value
  })
  require('../../../lib/utils/perf.js')
  process.emit('time', 'foo')
  process.emit('time', 'bar')
  setTimeout(() => {
    process.emit('timeEnd', 'foo')
    process.emit('timeEnd', 'bar')
    process.emit('time', 'foo')
    setTimeout(() => {
      process.emit('timeEnd', 'foo')
      process.emit('timeEnd', 'baz')
      t.match(logs, [
        ['timing', 'foo', /Completed in [0-9]+ms/],
        ['timing', 'bar', /Completed in [0-9]+ms/],
        ['timing', 'foo', /Completed in [0-9]+ms/],
        [
          'silly',
          'timing',
          "Tried to end timer that doesn't exist:",
          'baz',
        ],
      ])
      t.match(timings, { foo: Number, bar: Number })
      t.equal(timings.foo > timings.bar, true, 'foo should be > bar')
      t.end()
    }, 100)
  }, 100)
})
