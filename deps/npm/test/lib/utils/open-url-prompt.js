const t = require('tap')
const EventEmitter = require('events')
const tmock = require('../../fixtures/tmock')
const mockNpm = require('../../fixtures/mock-npm')

const mockOpenUrlPrompt = async (t, {
  questionShouldResolve = true,
  openUrlPromptInterrupted = false,
  openerResult = null,
  isTTY = true,
  emitter = null,
  url: openUrl = 'https://www.npmjs.com',
  ...config
}) => {
  const mock = await mockNpm(t, {
    globals: {
      'process.stdin.isTTY': isTTY,
      'process.stdout.isTTY': isTTY,
    },
    config,
  })

  let openerUrl = null
  let openerOpts = null

  const openUrlPrompt = tmock(t, '{LIB}/utils/open-url-prompt.js', {
    '@npmcli/promise-spawn': {
      open: async (url, options) => {
        openerUrl = url
        openerOpts = options
        if (openerResult) {
          throw openerResult
        }
      },
    },
    readline: {
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
    },
  })

  let error
  const args = [mock.npm, openUrl, 'npm home', 'prompt']
  if (emitter) {
    mock.open = openUrlPrompt(...args, emitter)
  } else {
    await openUrlPrompt(...args).catch((er) => error = er)
  }

  return {
    ...mock,
    openerUrl,
    openerOpts,
    OUTPUT: mock.joinedOutput(),
    emitter,
    error,
  }
}

t.test('does not open a url in non-interactive environments', async t => {
  const { openerUrl, openerOpts } = await mockOpenUrlPrompt(t, { isTTY: false })

  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
})

t.test('opens a url', async t => {
  const { OUTPUT, openerUrl, openerOpts } = await mockOpenUrlPrompt(t, { browser: true })

  t.equal(openerUrl, 'https://www.npmjs.com', 'opened the given url')
  t.same(openerOpts, { command: null }, 'passed command as null (the default)')
  t.matchSnapshot(OUTPUT)
})

t.test('opens a url with browser string', async t => {
  const { openerUrl, openerOpts } = await mockOpenUrlPrompt(t, { browser: 'firefox' })

  t.equal(openerUrl, 'https://www.npmjs.com', 'opened the given url')
  // FIXME: browser string is parsed as a boolean in config layer
  // this is a bug that should be fixed or the config should not allow it
  t.same(openerOpts, { command: null }, 'passed command as null (the default)')
})

t.test('prints json output', async t => {
  const { OUTPUT } = await mockOpenUrlPrompt(t, { json: true })

  t.matchSnapshot(OUTPUT)
})

t.test('returns error for non-https url', async t => {
  const { error, OUTPUT, openerUrl, openerOpts } = await mockOpenUrlPrompt(t, {
    url: 'ftp://www.npmjs.com',
  })

  t.match(error, /Invalid URL/, 'got the correct error')
  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
  t.same(OUTPUT, '', 'printed no output')
})

t.test('does not open url if canceled', async t => {
  const emitter = new EventEmitter()
  const { openerUrl, openerOpts, open } = await mockOpenUrlPrompt(t, {
    questionShouldResolve: false,
    emitter,
  })

  emitter.emit('abort')

  await open

  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
})

t.test('returns error when opener errors', async t => {
  const { error, openerUrl } = await mockOpenUrlPrompt(t, {
    openerResult: Object.assign(new Error('Opener failed'), { code: 1 }),
  })

  t.match(error, /Opener failed/, 'got the correct error')
  t.equal(openerUrl, 'https://www.npmjs.com', 'did not open')
})

t.test('does not error when opener can not find command', async t => {
  const { OUTPUT, error, openerUrl } = await mockOpenUrlPrompt(t, {
    // openerResult: new Error('Opener failed'),
    openerResult: Object.assign(new Error('Opener failed'), { code: 127 }),
  })

  t.notOk(error, 'Did not error')
  t.equal(openerUrl, 'https://www.npmjs.com', 'did not open')
  t.matchSnapshot(OUTPUT, 'Outputs extra Browser unavailable message and url')
})

t.test('throws "canceled" error on SIGINT', async t => {
  const emitter = new EventEmitter()
  const { open } = await mockOpenUrlPrompt(t, {
    questionShouldResolve: false,
    openUrlPromptInterrupted: true,
    emitter,
  })

  await t.rejects(open, /canceled/, 'message is canceled')
})
