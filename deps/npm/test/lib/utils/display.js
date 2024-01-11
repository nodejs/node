const t = require('tap')
const log = require('../../../lib/utils/log-shim')
const mockLogs = require('../../fixtures/mock-logs')
const mockGlobals = require('@npmcli/mock-globals')
const tmock = require('../../fixtures/tmock')
const util = require('util')

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

t.test('can log cleanly', async (t) => {
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

  display.log('error', 'test\x00message')
  t.match(logs.error, [['test^@message']])

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

class CustomObj {
  [util.inspect.custom] () {
    return this.inspected
  }
}

t.test('Display.clean', async (t) => {
  const Display = require('../../../lib/utils/display')
  const customNaN = new CustomObj()
  const customNull = new CustomObj()
  const customNumber = new CustomObj()
  const customObject = new CustomObj()
  const customString = new CustomObj()
  const customUndefined = new CustomObj()
  customNaN.inspected = NaN
  customNull.inspected = null
  customNumber.inspected = 477
  customObject.inspected = { custom: 'rend\x00ering' }
  customString.inspected = 'custom\x00rendering'
  customUndefined.inspected = undefined
  t.test('strings', async (t) => {
    const tests = [
      [477, '477'],
      [null, 'null'],
      [NaN, 'NaN'],
      [true, 'true'],
      [undefined, 'undefined'],
      ['🚀', '🚀'],
      // Cover the bounds of each range and a few characters from inside each range
      // \x00 through \x1f
      ['hello\x00world', 'hello^@world'],
      ['hello\x07world', 'hello^Gworld'],
      ['hello\x1bworld', 'hello^[world'],
      ['hello\x1eworld', 'hello^^world'],
      ['hello\x1fworld', 'hello^_world'],
      // \x7f is C0
      ['hello\x7fworld', 'hello^?world'],
      // \x80 through \x9f
      ['hello\x80world', 'hello^@world'],
      ['hello\x87world', 'hello^Gworld'],
      ['hello\x9eworld', 'hello^^world'],
      ['hello\x9fworld', 'hello^_world'],
      // Allowed C0
      ['hello\tworld', 'hello\tworld'],
      ['hello\nworld', 'hello\nworld'],
      ['hello\vworld', 'hello\vworld'],
      ['hello\rworld', 'hello\rworld'],
      // Allowed SGR
      ['hello\x1b[38;5;254mworld', 'hello\x1b[38;5;254mworld'],
      ['hello\x1b[mworld', 'hello\x1b[mworld'],
      // Unallowed CSI / OSC
      ['hello\x1b[2Aworld', 'hello^[[2Aworld'],
      ['hello\x9b[2Aworld', 'hello^[[2Aworld'],
      ['hello\x9decho goodbye\x9cworld', 'hello^]echo goodbye^\\world'],
      // This is done twice to ensure we define inspect.custom as writable
      [{ test: 'object' }, "{ test: 'object' }"],
      [{ test: 'object' }, "{ test: 'object' }"],
      // Make sure custom util.inspect doesn't bypass our cleaning
      [customNaN, 'NaN'],
      [customNull, 'null'],
      [customNumber, '477'],
      [customObject, "{ custom: 'rend\\x00ering' }"],
      [customString, 'custom^@rendering'],
      [customUndefined, 'undefined'],
      // UTF-16 form of 8-bit C1
      ['hello\xc2\x9bworld', 'hello\xc2^[world'],
    ]
    for (const [dirty, clean] of tests) {
      const cleaned = Display.clean(dirty)
      t.equal(util.format(cleaned), clean)
    }
  })
})
