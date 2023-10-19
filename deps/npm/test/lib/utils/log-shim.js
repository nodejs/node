const t = require('tap')
const tmock = require('../../fixtures/tmock')

const makeShim = (mocks) => tmock(t, '{LIB}/utils/log-shim.js', mocks)

const loggers = [
  'notice',
  'error',
  'warn',
  'info',
  'verbose',
  'http',
  'silly',
  'pause',
  'resume',
]

t.test('has properties', (t) => {
  const shim = makeShim()

  t.match(shim, {
    level: String,
    levels: {},
    gauge: {},
    stream: {},
    heading: undefined,
    enableColor: Function,
    disableColor: Function,
    enableUnicode: Function,
    disableUnicode: Function,
    enableProgress: Function,
    disableProgress: Function,
    ...loggers.reduce((acc, l) => {
      acc[l] = Function
      return acc
    }, {}),
  })

  t.match(Object.keys(shim).sort(), [
    'level',
    'heading',
    'levels',
    'gauge',
    'stream',
    'tracker',
    'useColor',
    'enableColor',
    'disableColor',
    'enableUnicode',
    'disableUnicode',
    'enableProgress',
    'disableProgress',
    'progressEnabled',
    'clearProgress',
    'showProgress',
    'newItem',
    'newGroup',
    ...loggers,
  ].sort())

  t.end()
})

t.test('works with npmlog/proclog proxy', t => {
  const procLog = { silly: () => 'SILLY' }
  const npmlog = { level: 'woo', enableColor: () => true }
  const shim = makeShim({ npmlog, 'proc-log': procLog })

  t.equal(shim.level, 'woo', 'can get a property')

  npmlog.level = 'hey'
  t.strictSame(
    [shim.level, npmlog.level],
    ['hey', 'hey'],
    'can get a property after update on npmlog'
  )

  shim.level = 'test'
  t.strictSame(
    [shim.level, npmlog.level],
    ['test', 'test'],
    'can get a property after update on shim'
  )

  t.ok(shim.enableColor(), 'can call method on shim to call npmlog')
  t.equal(shim.silly(), 'SILLY', 'can call method on proclog')
  t.notOk(shim.LEVELS, 'only includes levels from npmlog')
  t.throws(() => shim.gauge = 100, 'cant set getters properies')

  t.end()
})

t.test('works with npmlog/proclog proxy', t => {
  const shim = makeShim()

  loggers.forEach((k) => {
    t.doesNotThrow(() => shim[k]('test'))
  })

  t.end()
})
