const t = require('tap')
const mockGlobals = require('../../fixtures/mock-globals.js')
const EventEmitter = require('events')

const OUTPUT = []
const output = (...args) => OUTPUT.push(args)
const npm = {
  _config: {
    json: false,
    browser: true,
  },
  config: {
    get: k => npm._config[k],
    set: (k, v) => {
      npm._config[k] = v
    },
  },
  output,
}

let openerUrl = null
let openerOpts = null
let openerResult = null
const opener = (url, opts, cb) => {
  openerUrl = url
  openerOpts = opts
  return cb(openerResult)
}

let questionShouldResolve = true
let openUrlPromptInterrupted = false

const readline = {
  createInterface: () => ({
    question: (_q, cb) => {
      if (questionShouldResolve === true) {
        cb()
      }
    },
    close: () => {},
    on: (_signal, cb) => {
      if (openUrlPromptInterrupted && _signal === 'SIGINT') {
        cb()
      }
    },
  }),
}

const openUrlPrompt = t.mock('../../../lib/utils/open-url-prompt.js', {
  opener,
  readline,
})

mockGlobals(t, {
  'process.stdin.isTTY': true,
  'process.stdout.isTTY': true,
})

t.test('does not open a url in non-interactive environments', async t => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })

  mockGlobals(t, {
    'process.stdin.isTTY': false,
    'process.stdout.isTTY': false,
  })

  await openUrlPrompt(npm, 'https://www.npmjs.com', 'npm home', 'prompt')
  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
})

t.test('opens a url', async t => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm._config.browser = true
  })

  npm._config.browser = 'browser'
  await openUrlPrompt(npm, 'https://www.npmjs.com', 'npm home', 'prompt')
  t.equal(openerUrl, 'https://www.npmjs.com', 'opened the given url')
  t.same(openerOpts, { command: 'browser' }, 'passed command as null (the default)')
  t.matchSnapshot(OUTPUT)
})

t.test('prints json output', async t => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm._config.json = false
  })

  npm._config.json = true
  await openUrlPrompt(npm, 'https://www.npmjs.com', 'npm home', 'prompt')
  t.matchSnapshot(OUTPUT)
})

t.test('returns error for non-https url', async t => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })
  await t.rejects(
    openUrlPrompt(npm, 'ftp://www.npmjs.com', 'npm home', 'prompt'),
    /Invalid URL/,
    'got the correct error'
  )
  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
  t.same(OUTPUT, [], 'printed no output')
})

t.test('does not open url if canceled', async t => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    questionShouldResolve = true
  })

  questionShouldResolve = false
  const emitter = new EventEmitter()

  const open = openUrlPrompt(npm, 'https://www.npmjs.com', 'npm home', 'prompt', emitter)

  emitter.emit('abort')

  await open

  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
})

t.test('returns error when opener errors', async t => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    openerResult = null
    OUTPUT.length = 0
  })

  openerResult = new Error('Opener failed')

  await t.rejects(
    openUrlPrompt(npm, 'https://www.npmjs.com', 'npm home', 'prompt'),
    /Opener failed/,
    'got the correct error'
  )
  t.equal(openerUrl, 'https://www.npmjs.com', 'did not open')
})

t.test('throws "canceled" error on SIGINT', async t => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    questionShouldResolve = true
    openUrlPromptInterrupted = false
  })

  questionShouldResolve = false
  openUrlPromptInterrupted = true
  const emitter = new EventEmitter()

  const open = openUrlPrompt(npm, 'https://www.npmjs.com', 'npm home', 'prompt', emitter)

  try {
    await open
  } catch (err) {
    t.equal(err.message, 'canceled')
  }
})
