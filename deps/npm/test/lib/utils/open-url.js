const { test } = require('tap')
const requireInject = require('require-inject')

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
}

const OUTPUT = []
const output = (...args) => OUTPUT.push(args)

let openerUrl = null
let openerOpts = null
let openerResult = null
const opener = (url, opts, cb) => {
  openerUrl = url
  openerOpts = opts
  return cb(openerResult)
}

const openUrl = requireInject('../../../lib/utils/open-url.js', {
  '../../../lib/npm.js': npm,
  '../../../lib/utils/output.js': output,
  opener,
})

test('opens a url', (t) => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })
  openUrl('https://www.npmjs.com', 'npm home', (err) => {
    if (err)
      throw err

    t.equal(openerUrl, 'https://www.npmjs.com', 'opened the given url')
    t.same(openerOpts, { command: null }, 'passed command as null (the default)')
    t.same(OUTPUT, [], 'printed no output')
    t.done()
  })
})

test('returns error for non-https and non-file url', (t) => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })
  openUrl('ftp://www.npmjs.com', 'npm home', (err) => {
    t.match(err, /Invalid URL/, 'got the correct error')
    t.equal(openerUrl, null, 'did not open')
    t.same(openerOpts, null, 'did not open')
    t.same(OUTPUT, [], 'printed no output')
    t.done()
  })
})

test('returns error for non-parseable url', (t) => {
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
  })
  openUrl('git+ssh://user@host:repo.git', 'npm home', (err) => {
    t.match(err, /Invalid URL/, 'got the correct error')
    t.equal(openerUrl, null, 'did not open')
    t.same(openerOpts, null, 'did not open')
    t.same(OUTPUT, [], 'printed no output')
    t.done()
  })
})

test('opens a url with the given browser', (t) => {
  npm.config.set('browser', 'chrome')
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
  })
  openUrl('https://www.npmjs.com', 'npm home', (err) => {
    if (err)
      throw err

    t.equal(openerUrl, 'https://www.npmjs.com', 'opened the given url')
    t.same(openerOpts, { command: 'chrome' }, 'passed the given browser as command')
    t.same(OUTPUT, [], 'printed no output')
    t.done()
  })
})

test('prints where to go when browser is disabled', (t) => {
  npm.config.set('browser', false)
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
  })
  openUrl('https://www.npmjs.com', 'npm home', (err) => {
    if (err)
      throw err

    t.equal(openerUrl, null, 'did not open')
    t.same(openerOpts, null, 'did not open')
    t.equal(OUTPUT.length, 1, 'got one logged message')
    t.equal(OUTPUT[0].length, 1, 'logged message had one value')
    t.matchSnapshot(OUTPUT[0][0], 'printed expected message')
    t.done()
  })
})

test('prints where to go when browser is disabled and json is enabled', (t) => {
  npm.config.set('browser', false)
  npm.config.set('json', true)
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
    npm.config.set('json', false)
  })
  openUrl('https://www.npmjs.com', 'npm home', (err) => {
    if (err)
      throw err

    t.equal(openerUrl, null, 'did not open')
    t.same(openerOpts, null, 'did not open')
    t.equal(OUTPUT.length, 1, 'got one logged message')
    t.equal(OUTPUT[0].length, 1, 'logged message had one value')
    t.matchSnapshot(OUTPUT[0][0], 'printed expected message')
    t.done()
  })
})

test('prints where to go when given browser does not exist', (t) => {
  npm.config.set('browser', 'firefox')
  openerResult = Object.assign(new Error('failed'), { code: 'ENOENT' })
  t.teardown(() => {
    openerUrl = null
    openerOpts = null
    OUTPUT.length = 0
    npm.config.set('browser', true)
  })
  openUrl('https://www.npmjs.com', 'npm home', (err) => {
    if (err)
      throw err

    t.equal(openerUrl, 'https://www.npmjs.com', 'tried to open the correct url')
    t.same(openerOpts, { command: 'firefox' }, 'tried to use the correct browser')
    t.equal(OUTPUT.length, 1, 'got one logged message')
    t.equal(OUTPUT[0].length, 1, 'logged message had one value')
    t.matchSnapshot(OUTPUT[0][0], 'printed expected message')
    t.done()
  })
})
