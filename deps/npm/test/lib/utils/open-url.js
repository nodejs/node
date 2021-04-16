const t = require('tap')

const OUTPUT = []
const output = (...args) => OUTPUT.push(args)
const npm = {
  _config: {
    json: false,
    browser: true,
  },
  config: {
    get: (k) => npm._config[k],
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

const openUrl = t.mock('../../../lib/utils/open-url.js', {
  opener,
})

t.test('opens a url', async (t) => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })
  await openUrl(npm, 'https://www.npmjs.com', 'npm home')
  t.equal(openerUrl, 'https://www.npmjs.com', 'opened the given url')
  t.same(openerOpts, { command: null }, 'passed command as null (the default)')
  t.same(OUTPUT, [], 'printed no output')
})

t.test('returns error for non-https and non-file url', async (t) => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })
  t.rejects(openUrl(npm, 'ftp://www.npmjs.com', 'npm home'), /Invalid URL/, 'got the correct error')
  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
  t.same(OUTPUT, [], 'printed no output')
  t.end()
})

t.test('returns error for non-parseable url', async (t) => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })
  t.rejects(openUrl(npm, 'git+ssh://user@host:repo.git', 'npm home'), /Invalid URL/, 'got the correct error')
  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
  t.same(OUTPUT, [], 'printed no output')
  t.end()
})

t.test('opens a url with the given browser', async (t) => {
  npm.config.set('browser', 'chrome')
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
  })
  await openUrl(npm, 'https://www.npmjs.com', 'npm home')
  t.equal(openerUrl, 'https://www.npmjs.com', 'opened the given url')
  t.same(openerOpts, { command: 'chrome' }, 'passed the given browser as command')
  t.same(OUTPUT, [], 'printed no output')
  t.end()
})

t.test('prints where to go when browser is disabled', async (t) => {
  npm.config.set('browser', false)
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
  })
  await openUrl(npm, 'https://www.npmjs.com', 'npm home')
  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
  t.equal(OUTPUT.length, 1, 'got one logged message')
  t.equal(OUTPUT[0].length, 1, 'logged message had one value')
  t.matchSnapshot(OUTPUT[0][0], 'printed expected message')
  t.end()
})

t.test('prints where to go when browser is disabled and json is enabled', async (t) => {
  npm.config.set('browser', false)
  npm.config.set('json', true)
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
    npm.config.set('json', false)
  })
  await openUrl(npm, 'https://www.npmjs.com', 'npm home')
  t.equal(openerUrl, null, 'did not open')
  t.same(openerOpts, null, 'did not open')
  t.equal(OUTPUT.length, 1, 'got one logged message')
  t.equal(OUTPUT[0].length, 1, 'logged message had one value')
  t.matchSnapshot(OUTPUT[0][0], 'printed expected message')
  t.end()
})

t.test('prints where to go when given browser does not exist', async (t) => {
  npm.config.set('browser', 'firefox')
  openerResult = Object.assign(new Error('failed'), { code: 'ENOENT' })
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
  })
  await openUrl(npm, 'https://www.npmjs.com', 'npm home')
  t.equal(openerUrl, 'https://www.npmjs.com', 'tried to open the correct url')
  t.same(openerOpts, { command: 'firefox' }, 'tried to use the correct browser')
  t.equal(OUTPUT.length, 1, 'got one logged message')
  t.equal(OUTPUT[0].length, 1, 'logged message had one value')
  t.matchSnapshot(OUTPUT[0][0], 'printed expected message')
  t.end()
})

t.test('handles unknown opener error', async (t) => {
  npm.config.set('browser', 'firefox')
  openerResult = Object.assign(new Error('failed'), { code: 'ENOBRIAN' })
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
  })
  t.rejects(openUrl(npm, 'https://www.npmjs.com', 'npm home'), 'failed', 'got the correct error')
  t.end()
})
