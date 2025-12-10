const t = require('tap')
const timers = require('node:timers/promises')
const tmock = require('../../fixtures/tmock')
const mockLogs = require('../../fixtures/mock-logs')
const mockGlobals = require('@npmcli/mock-globals')
const { inspect } = require('node:util')

const mockDisplay = async (t, { mocks, load } = {}) => {
  const procLog = require('proc-log')

  const logs = mockLogs()

  const Display = tmock(t, '{LIB}/utils/display', mocks)
  const display = new Display(logs.streams)
  const displayLoad = async (opts) => display.load({
    loglevel: 'silly',
    stderrColor: false,
    stdoutColor: false,
    heading: 'npm',
    ...opts,
  })

  if (load !== false) {
    await displayLoad(load)
  }

  t.teardown(() => display.off())
  return {
    ...procLog,
    display,
    displayLoad,
    ...logs.logs,
  }
}

t.test('can log cleanly', async (t) => {
  const { log, logs } = await mockDisplay(t)

  log.error('', 'test\x00message')
  log.info('', 'fetch DELETE 200 https://registry.npmjs.org/-/user/token/npm_000000000000000000000000000000000000 477ms')
  t.match(logs.error, ['test^@message'])
  t.match(logs.info, ['fetch DELETE 200 https://registry.npmjs.org/-/user/token/npm_*** 477ms'])
})

t.test('can handle special eresolves', async (t) => {
  const explains = []
  const { log, logs } = await mockDisplay(t, {
    mocks: {
      '{LIB}/utils/explain-eresolve.js': {
        explain: (...args) => {
          explains.push(args)
          return 'EXPLAIN'
        },
      },
    },
  })

  log.warn('ERESOLVE', 'hello', { some: 'object' })
  t.strictSame(logs.warn, ['ERESOLVE hello', 'EXPLAIN'])
  t.match(explains, [[{ some: 'object' }, Function, 2]])
})

t.test('can buffer output when paused', async t => {
  const { displayLoad, outputs, output } = await mockDisplay(t, {
    load: false,
  })

  output.buffer('Message 1')
  output.standard('Message 2')

  t.strictSame(outputs, [])
  await displayLoad()
  t.strictSame(outputs, ['Message 1', 'Message 2'])
})

t.test('can do progress', async (t) => {
  const { log, logs, outputs, outputErrors, output, input } = await mockDisplay(t, {
    load: {
      progress: true,
    },
  })

  // wait for initial timer interval to load
  await timers.setTimeout(200)

  log.error('', 'before input')
  output.standard('before input')

  const end = input.start()
  log.error('', 'during input')
  output.standard('during input')
  end()

  // wait long enough for all spinner frames to render
  await timers.setTimeout(800)
  log.error('', 'after input')
  output.standard('after input')

  t.strictSame([...new Set(outputErrors)].sort(), ['-', '/', '\\', '|'])
  t.strictSame(logs, ['error before input', 'error during input', 'error after input'])
  t.strictSame(outputs, ['before input', 'during input', 'after input'])
})

t.test('handles log throwing', async (t) => {
  class ThrowInspect {
    #crashes = 0;

    [inspect.custom] () {
      throw new Error(`Crashed ${++this.#crashes}`)
    }
  }

  const errors = []
  mockGlobals(t, { 'console.error': (...msg) => errors.push(msg) })

  const { log, logs } = await mockDisplay(t)

  log.error('woah', new ThrowInspect())

  t.strictSame(logs.error, [])
  t.equal(errors.length, 1)
  t.match(errors[0], [
    'attempt to log crashed',
    new Error('Crashed 1'),
    new Error('Crashed 2'),
  ])
})

t.test('incorrect levels', async t => {
  const { outputs } = await mockDisplay(t)
  process.emit('output', 'not a real level')
  t.strictSame(outputs, [], 'output is ignored')
})

t.test('notice deduplication', async t => {
  const { log, logs, display } = await mockDisplay(t, {
    load: { loglevel: 'notice' },
  })

  // Log the same notice multiple times - should be deduplicated
  log.notice('', 'This is a duplicate notice')
  log.notice('', 'This is a duplicate notice')
  log.notice('', 'This is a duplicate notice')

  // Should only appear once in logs
  t.equal(logs.notice.length, 1, 'notice appears only once')
  t.strictSame(logs.notice, ['This is a duplicate notice'])

  // Different notice should appear
  log.notice('', 'This is a different notice')
  t.equal(logs.notice.length, 2, 'different notice is shown')
  t.strictSame(logs.notice, [
    'This is a duplicate notice',
    'This is a different notice',
  ])

  // Same notice with different title should appear
  log.notice('title', 'This is a duplicate notice')
  t.equal(logs.notice.length, 3, 'notice with different title is shown')
  t.match(logs.notice[2], /title.*This is a duplicate notice/)

  // Log the first notice again - should still be deduplicated
  log.notice('', 'This is a duplicate notice')
  t.equal(logs.notice.length, 3, 'original notice still deduplicated')

  // Call off() to simulate end of command and clear deduplication
  display.off()

  // Create a new display instance to simulate a new command
  const logsResult = mockLogs()
  const Display = tmock(t, '{LIB}/utils/display')
  const display2 = new Display(logsResult.streams)
  await display2.load({
    loglevel: 'silly',
    stderrColor: false,
    stdoutColor: false,
    heading: 'npm',
  })
  t.teardown(() => display2.off())

  // Log the same notice again - should appear since deduplication was cleared
  log.notice('', 'This is a duplicate notice')
  t.equal(logsResult.logs.logs.notice.length, 1, 'notice appears after display.off() clears deduplication')
  t.strictSame(logsResult.logs.logs.notice, ['This is a duplicate notice'])
})

t.test('notice deduplication does not apply in verbose mode', async t => {
  const { log, logs } = await mockDisplay(t, {
    load: { loglevel: 'verbose' },
  })

  // Log the same notice multiple times in verbose mode
  log.notice('', 'Repeated notice')
  log.notice('', 'Repeated notice')
  log.notice('', 'Repeated notice')

  // Should appear all three times in verbose mode
  t.equal(logs.notice.length, 3, 'all notices appear in verbose mode')
  t.strictSame(logs.notice, [
    'Repeated notice',
    'Repeated notice',
    'Repeated notice',
  ])
})

t.test('Display.clean', async (t) => {
  const { output, outputs, clearOutput } = await mockDisplay(t)

  class CustomObj {
    #inspected

    constructor (val) {
      this.#inspected = val
    }

    [inspect.custom] () {
      return this.#inspected
    }
  }

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
    // Make sure custom util.inspect doesn't bypass our cleaning
    [new CustomObj(NaN), 'NaN'],
    [new CustomObj(null), 'null'],
    [new CustomObj(477), '477'],
    [new CustomObj({ custom: 'rend\x00ering' }), "{ custom: 'rend\\x00ering' }"],
    [new CustomObj('custom\x00rendering'), 'custom^@rendering'],
    [new CustomObj(undefined), 'undefined'],
    // UTF-16 form of 8-bit C1
    ['hello\xc2\x9bworld', 'hello\xc2^[world'],
  ]

  for (const [dirty, clean] of tests) {
    output.standard(dirty)
    t.equal(outputs[0], clean)
    clearOutput()
  }
})

t.test('prompt functionality', async t => {
  t.test('regular prompt completion works', async t => {
    const { input } = await mockDisplay(t)

    const result = await input.read(() => Promise.resolve('user-input'))

    t.equal(result, 'user-input', 'should return the input result')
  })

  t.test('silent prompt completion works', async t => {
    const { input } = await mockDisplay(t)

    const result = await input.read(
      () => Promise.resolve('secret-password'),
      { silent: true }
    )

    t.equal(result, 'secret-password', 'should return the input result for silent prompts')
  })

  t.test('metadata is correctly passed through', async t => {
    const { input } = await mockDisplay(t)

    await input.read(
      () => Promise.resolve('result1'),
      { silent: false }
    )
    t.pass('should handle silent false option')

    await input.read(
      () => Promise.resolve('result2'),
      {}
    )
    t.pass('should handle empty options')

    await input.read(
      () => Promise.resolve('result3'),
      { silent: true }
    )
    t.pass('should handle silent true option')
  })
})
