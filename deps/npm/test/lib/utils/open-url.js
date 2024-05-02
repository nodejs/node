const t = require('tap')
const tmock = require('../../fixtures/tmock')
const mockNpm = require('../../fixtures/mock-npm')

const mockOpenUrl = async (t, args, { openerResult, ...config } = {}) => {
  let openerUrl = null
  let openerOpts = null

  const open = async (url, options) => {
    openerUrl = url
    openerOpts = options
    if (openerResult) {
      throw openerResult
    }
  }

  const mock = await mockNpm(t, { config })

  const openUrl = tmock(t, '{LIB}/utils/open-url.js', {
    '@npmcli/promise-spawn': { open },
  })

  const openWithNpm = (...a) => openUrl(mock.npm, ...a)

  if (args) {
    await openWithNpm(...args)
  }

  return {
    ...mock,
    openUrl: openWithNpm,
    openerUrl: () => openerUrl,
    openerOpts: () => openerOpts,
  }
}

t.test('opens a url', async t => {
  const { openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t,
    ['https://www.npmjs.com', 'npm home'])
  t.equal(openerUrl(), 'https://www.npmjs.com', 'opened the given url')
  t.same(openerOpts(), { command: null }, 'passed command as null (the default)')
  t.same(joinedOutput(), '', 'printed no output')
})

t.test('returns error for non-https url', async t => {
  const { openUrl, openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t)
  await t.rejects(
    openUrl('ftp://www.npmjs.com', 'npm home'),
    /Invalid URL/,
    'got the correct error'
  )
  t.equal(openerUrl(), null, 'did not open')
  t.same(openerOpts(), null, 'did not open')
  t.same(joinedOutput(), '', 'printed no output')
})

t.test('returns error for file url', async t => {
  const { openUrl, openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t)
  await t.rejects(
    openUrl('file:///usr/local/bin/ls', 'npm home'),
    /Invalid URL/,
    'got the correct error'
  )
  t.equal(openerUrl(), null, 'did not open')
  t.same(openerOpts(), null, 'did not open')
  t.same(joinedOutput(), '', 'printed no output')
})

t.test('file url allowed if explicitly asked for', async t => {
  const { openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t,
    ['file:///man/page/npm-install', 'npm home', true])
  t.equal(openerUrl(), 'file:///man/page/npm-install', 'opened the given url')
  t.same(openerOpts(), { command: null }, 'passed command as null (the default)')
  t.same(joinedOutput(), '', 'printed no output')
})

t.test('returns error for non-parseable url', async t => {
  const { openUrl, openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t)
  await t.rejects(
    openUrl('git+ssh://user@host:repo.git', 'npm home'),
    /Invalid URL/,
    'got the correct error'
  )
  t.equal(openerUrl(), null, 'did not open')
  t.same(openerOpts(), null, 'did not open')
  t.same(joinedOutput(), '', 'printed no output')
})

t.test('encodes non-URL-safe characters in url provided', async t => {
  const { openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t,
    ['https://www.npmjs.com/|cat', 'npm home'])
  t.equal(openerUrl(), 'https://www.npmjs.com/%7Ccat', 'opened the encoded url')
  t.same(openerOpts(), { command: null }, 'passed command as null (the default)')
  t.same(joinedOutput(), '', 'printed no output')
})

t.test('opens a url with the given browser', async t => {
  const { openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t,
    ['https://www.npmjs.com', 'npm home'], { browser: 'chrome' })
  t.equal(openerUrl(), 'https://www.npmjs.com', 'opened the given url')
  // FIXME: browser string is parsed as a boolean in config layer
  // this is a bug that should be fixed or the config should not allow it
  t.same(openerOpts(), { command: null }, 'passed the given browser as command')
  t.same(joinedOutput(), '', 'printed no output')
})

t.test('prints where to go when browser is disabled', async t => {
  const { openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t,
    ['https://www.npmjs.com', 'npm home'], { browser: false })
  t.equal(openerUrl(), null, 'did not open')
  t.same(openerOpts(), null, 'did not open')
  t.matchSnapshot(joinedOutput(), 'printed expected message')
})

t.test('prints where to go when browser is disabled and json is enabled', async t => {
  const { openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t,
    ['https://www.npmjs.com', 'npm home'], { browser: false, json: true })
  t.equal(openerUrl(), null, 'did not open')
  t.same(openerOpts(), null, 'did not open')
  t.matchSnapshot(joinedOutput(), 'printed expected message')
})

t.test('prints where to go when given browser does not exist', async t => {
  const { openerUrl, openerOpts, joinedOutput } = await mockOpenUrl(t,
    ['https://www.npmjs.com', 'npm home'],
    {
      openerResult: Object.assign(new Error('failed'), { code: 127 }),
    }
  )

  t.equal(openerUrl(), 'https://www.npmjs.com', 'tried to open the correct url')
  t.same(openerOpts(), { command: null }, 'tried to use the correct browser')
  t.matchSnapshot(joinedOutput(), 'printed expected message')
})

t.test('handles unknown opener error', async t => {
  const { openUrl } = await mockOpenUrl(t, null, {
    browser: 'firefox',
    openerResult: Object.assign(new Error('failed'), { code: 'ENOBRIAN' }),
  })

  await t.rejects(openUrl('https://www.npmjs.com', 'npm home'), 'failed', 'got the correct error')
})
