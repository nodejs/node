const t = require('tap')
const { resolve, join } = require('path')
const fs = require('graceful-fs')
const mockLogs = require('../../fixtures/mock-logs')

const mockTimers = (t, options) => {
  const { logs, logMocks } = mockLogs()
  const Timers = t.mock('../../../lib/utils/timers', {
    ...logMocks,
  })
  const timers = new Timers(options)
  t.teardown(() => timers.off())
  return { timers, logs }
}

t.test('getters', async (t) => {
  const { timers } = mockTimers(t)
  t.match(timers.unfinished, new Map())
  t.match(timers.finished, {})
})

t.test('listens/stops on process', async (t) => {
  const { timers } = mockTimers(t)
  process.emit('time', 'foo')
  process.emit('time', 'bar')
  process.emit('timeEnd', 'bar')
  t.match(timers.unfinished, new Map([['foo', Number]]))
  t.match(timers.finished, { bar: Number })
  timers.off()
  process.emit('time', 'baz')
  t.notOk(timers.unfinished.get('baz'))
})

t.test('convenience time method', async (t) => {
  const { timers } = mockTimers(t)

  const end = timers.time('later')
  timers.time('sync', () => {})
  await timers.time('async', () => new Promise(r => setTimeout(r, 10)))
  end()

  t.match(timers.finished, { later: Number, sync: Number, async: Number })
})

t.test('initial timer', async (t) => {
  const { timers } = mockTimers(t, { start: 'foo' })
  process.emit('timeEnd', 'foo')
  t.match(timers.finished, { foo: Number })
})

t.test('initial listener', async (t) => {
  const events = []
  const listener = (...args) => events.push(args)
  const { timers } = mockTimers(t, { listener })
  process.emit('time', 'foo')
  process.emit('time', 'bar')
  process.emit('timeEnd', 'bar')
  timers.off(listener)
  process.emit('timeEnd', 'foo')
  t.equal(events.length, 1)
  t.match(events, [['bar', Number]])
})

t.test('finish unstarted timer', async (t) => {
  const { logs } = mockTimers(t)
  process.emit('timeEnd', 'foo')
  t.match(logs.silly, [['timing', /^Tried to end timer/, 'foo']])
})

t.test('writes file', async (t) => {
  const { timers } = mockTimers(t)
  const dir = t.testdir()
  process.emit('time', 'foo')
  process.emit('timeEnd', 'foo')
  timers.load({ dir })
  timers.writeFile({ some: 'data' })
  const data = JSON.parse(fs.readFileSync(resolve(dir, '_timing.json')))
  t.match(data, {
    some: 'data',
    foo: Number,
    unfinished: {
      npm: [Number, Number],
    },
  })
})

t.test('fails to write file', async (t) => {
  const { logs, timers } = mockTimers(t)
  const dir = t.testdir()

  timers.load({ dir: join(dir, 'does', 'not', 'exist') })
  timers.writeFile()

  t.match(logs.warn, [['timing', 'could not write timing file']])
  t.equal(timers.file, null)
})

t.test('no dir and no file', async (t) => {
  const { logs, timers } = mockTimers(t)

  timers.load()
  timers.writeFile()

  t.strictSame(logs, [])
  t.equal(timers.file, null)
})
