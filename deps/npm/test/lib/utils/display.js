const t = require('tap')
const log = require('../../../lib/utils/log-shim')
const mockLogs = require('../../fixtures/mock-logs')
const mockGlobals = require('@npmcli/mock-globals')
const tmock = require('../../fixtures/tmock')

const mockDisplay = (t, mocks) => {
  const { logs, logMocks } = mockLogs(mocks)
  const Display = tmock(t, '{LIB}/utils/display', {
    ...mocks,
    ...logMocks,
  })
  const display = new Display()
  t.teardown(() => display.off())
  return { display, logs }
}

t.test('setup', async (t) => {
  const { display } = mockDisplay(t)

  display.load({ timing: true, loglevel: 'notice' })
  t.equal(log.level, 'notice')

  display.load({ timing: false, loglevel: 'notice' })
  t.equal(log.level, 'notice')

  display.load({ color: true })
  t.equal(log.useColor(), true)

  display.load({ unicode: true })
  t.equal(log.gauge._theme.hasUnicode, true)

  display.load({ unicode: false })
  t.equal(log.gauge._theme.hasUnicode, false)

  mockGlobals(t, { 'process.stderr.isTTY': true })
  display.load({ progress: true })
  t.equal(log.progressEnabled, true)
})

t.test('can log', async (t) => {
  const explains = []
  const { display, logs } = mockDisplay(t, {
    npmlog: {
      error: (...args) => logs.push(['error', ...args]),
      warn: (...args) => logs.push(['warn', ...args]),
    },
    '{LIB}/utils/explain-eresolve.js': {
      explain: (...args) => {
        explains.push(args)
        return 'explanation'
      },
    },
  })

  display.log('error', 'test')
  t.match(logs.error, [['test']])

  display.log('warn', 'ERESOLVE', 'hello', { some: 'object' })
  t.match(logs.warn, [['ERESOLVE', 'hello']])
  t.match(explains, [[{ some: 'object' }, null, 2]])
})

t.test('handles log throwing', async (t) => {
  const errors = []
  mockGlobals(t, {
    'console.error': (...args) => errors.push(args),
  })
  const { display } = mockDisplay(t, {
    npmlog: {
      verbose: () => {
        throw new Error('verbose')
      },
    },
    '{LIB}/utils/explain-eresolve.js': {
      explain: () => {
        throw new Error('explain')
      },
    },
  })

  display.log('warn', 'ERESOLVE', 'hello', { some: 'object' })
  t.match(errors, [
    [/attempt to log .* crashed/, Error('explain'), Error('verbose')],
  ])
})
