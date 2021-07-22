const t = require('tap')

const settings = {
  level: 'warn',
}
t.afterEach(() => {
  Object.keys(settings).forEach(k => {
    delete settings[k]
  })
})

const WARN_CALLED = []
const npmlog = {
  warn: (...args) => {
    WARN_CALLED.push(args)
  },
  levels: {
    silly: -Infinity,
    verbose: 1000,
    info: 2000,
    timing: 2500,
    http: 3000,
    notice: 3500,
    warn: 4000,
    error: 5000,
    silent: Infinity,
  },
  settings,
  enableColor: () => {
    settings.color = true
  },
  disableColor: () => {
    settings.color = false
  },
  enableUnicode: () => {
    settings.unicode = true
  },
  disableUnicode: () => {
    settings.unicode = false
  },
  enableProgress: () => {
    settings.progress = true
  },
  disableProgress: () => {
    settings.progress = false
  },
  get heading () {
    return settings.heading
  },
  set heading (h) {
    settings.heading = h
  },
  get level () {
    return settings.level
  },
  set level (l) {
    settings.level = l
  },
}

const EXPLAIN_CALLED = []
const setupLog = t.mock('../../../lib/utils/setup-log.js', {
  '../../../lib/utils/explain-eresolve.js': {
    explain: (...args) => {
      EXPLAIN_CALLED.push(args)
      return 'explanation'
    },
  },
  npmlog,
})

const config = obj => ({
  get (k) {
    return obj[k]
  },
  set (k, v) {
    obj[k] = v
  },
})

t.test('setup with color=always and unicode', t => {
  npmlog.warn('ERESOLVE', 'hello', { some: 'object' })
  t.strictSame(EXPLAIN_CALLED, [], 'log.warn() not patched yet')
  t.strictSame(WARN_CALLED, [['ERESOLVE', 'hello', { some: 'object' }]])
  WARN_CALLED.length = 0

  t.equal(setupLog(config({
    loglevel: 'warn',
    color: 'always',
    unicode: true,
    progress: false,
  })), true)

  npmlog.warn('ERESOLVE', 'hello', { some: { other: 'object' } })
  t.strictSame(EXPLAIN_CALLED, [[{ some: { other: 'object' } }, true, 2]],
    'log.warn(ERESOLVE) patched to call explainEresolve()')
  t.strictSame(WARN_CALLED, [
    ['ERESOLVE', 'hello'],
    ['', 'explanation'],
  ], 'warn the explanation')
  EXPLAIN_CALLED.length = 0
  WARN_CALLED.length = 0
  npmlog.warn('some', 'other', 'thing')
  t.strictSame(EXPLAIN_CALLED, [], 'do not try to explain other things')
  t.strictSame(WARN_CALLED, [['some', 'other', 'thing']], 'warnings passed through')

  t.strictSame(settings, {
    level: 'warn',
    color: true,
    unicode: true,
    progress: false,
    heading: 'npm',
  })

  t.end()
})

t.test('setup with color=true, no unicode, and non-TTY terminal', t => {
  const { isTTY: stderrIsTTY } = process.stderr
  const { isTTY: stdoutIsTTY } = process.stdout
  t.teardown(() => {
    process.stderr.isTTY = stderrIsTTY
    process.stdout.isTTY = stdoutIsTTY
  })
  process.stderr.isTTY = false
  process.stdout.isTTY = false

  t.equal(setupLog(config({
    loglevel: 'warn',
    color: false,
    progress: false,
    heading: 'asdf',
  })), false)

  t.strictSame(settings, {
    level: 'warn',
    color: false,
    unicode: false,
    progress: false,
    heading: 'asdf',
  })

  t.end()
})

t.test('setup with color=true, no unicode, and dumb TTY terminal', t => {
  const { isTTY: stderrIsTTY } = process.stderr
  const { isTTY: stdoutIsTTY } = process.stdout
  const { TERM } = process.env
  t.teardown(() => {
    process.stderr.isTTY = stderrIsTTY
    process.stdout.isTTY = stdoutIsTTY
    process.env.TERM = TERM
  })
  process.stderr.isTTY = true
  process.stdout.isTTY = true
  process.env.TERM = 'dumb'

  t.equal(setupLog(config({
    loglevel: 'warn',
    color: true,
    progress: false,
    heading: 'asdf',
  })), true)

  t.strictSame(settings, {
    level: 'warn',
    color: true,
    unicode: false,
    progress: false,
    heading: 'asdf',
  })

  t.end()
})

t.test('setup with color=true, no unicode, and non-dumb TTY terminal', t => {
  const { isTTY: stderrIsTTY } = process.stderr
  const { isTTY: stdoutIsTTY } = process.stdout
  const { TERM } = process.env
  t.teardown(() => {
    process.stderr.isTTY = stderrIsTTY
    process.stdout.isTTY = stdoutIsTTY
    process.env.TERM = TERM
  })
  process.stderr.isTTY = true
  process.stdout.isTTY = true
  process.env.TERM = 'totes not dum'

  t.equal(setupLog(config({
    loglevel: 'warn',
    color: true,
    progress: true,
    heading: 'asdf',
  })), true)

  t.strictSame(settings, {
    level: 'warn',
    color: true,
    unicode: false,
    progress: true,
    heading: 'asdf',
  })

  t.end()
})

t.test('setup with non-TTY stdout, TTY stderr', t => {
  const { isTTY: stderrIsTTY } = process.stderr
  const { isTTY: stdoutIsTTY } = process.stdout
  const { TERM } = process.env
  t.teardown(() => {
    process.stderr.isTTY = stderrIsTTY
    process.stdout.isTTY = stdoutIsTTY
    process.env.TERM = TERM
  })
  process.stderr.isTTY = true
  process.stdout.isTTY = false
  process.env.TERM = 'definitely not a dummy'

  t.equal(setupLog(config({
    loglevel: 'warn',
    color: true,
    progress: true,
    heading: 'asdf',
  })), false)

  t.strictSame(settings, {
    level: 'warn',
    color: true,
    unicode: false,
    progress: true,
    heading: 'asdf',
  })

  t.end()
})

t.test('setup with TTY stdout, non-TTY stderr', t => {
  const { isTTY: stderrIsTTY } = process.stderr
  const { isTTY: stdoutIsTTY } = process.stdout
  const { TERM } = process.env
  t.teardown(() => {
    process.stderr.isTTY = stderrIsTTY
    process.stdout.isTTY = stdoutIsTTY
    process.env.TERM = TERM
  })
  process.stderr.isTTY = false
  process.stdout.isTTY = true

  t.equal(setupLog(config({
    loglevel: 'warn',
    color: true,
    progress: true,
    heading: 'asdf',
  })), true)

  t.strictSame(settings, {
    level: 'warn',
    color: false,
    unicode: false,
    progress: false,
    heading: 'asdf',
  })

  t.end()
})

t.test('set loglevel to timing', t => {
  setupLog(config({
    timing: true,
    loglevel: 'notice',
  }))
  t.equal(settings.level, 'timing')
  t.end()
})

t.test('silent has no logging', t => {
  const { isTTY: stderrIsTTY } = process.stderr
  const { isTTY: stdoutIsTTY } = process.stdout
  const { TERM } = process.env
  t.teardown(() => {
    process.stderr.isTTY = stderrIsTTY
    process.stdout.isTTY = stdoutIsTTY
    process.env.TERM = TERM
  })
  process.stderr.isTTY = true
  process.stdout.isTTY = true
  process.env.TERM = 'totes not dum'

  setupLog(config({
    loglevel: 'silent',
  }))
  t.equal(settings.progress, false, 'progress disabled when silent')
  t.end()
})
