const requireInject = require('require-inject')
const t = require('tap')

let result = ''

const noop = () => null
const npm = { config: { get () {} }, flatOptions: { unicode: false } }
const npmFetch = { json: noop }
const npmlog = { error: noop, info: noop, verbose: noop }
const mocks = {
  npmlog,
  'npm-registry-fetch': npmFetch,
  '../../lib/npm.js': npm,
  '../../lib/utils/output.js': (...msg) => {
    result += msg.join('\n')
  },
  '../../lib/utils/get-identity.js': async () => 'foo',
  '../../lib/utils/usage.js': () => 'usage instructions',
}

const star = requireInject('../../lib/star.js', mocks)

t.afterEach(cb => {
  npm.config = { get () {} }
  npm.flatOptions.unicode = false
  npmlog.info = noop
  result = ''
  cb()
})

t.test('no args', t => {
  star([], err => {
    t.match(
      err,
      /usage instructions/,
      'should throw usage instructions'
    )
    t.end()
  })
})

t.test('star a package', t => {
  t.plan(4)
  const pkgName = '@npmcli/arborist'
  npmFetch.json = async (uri, opts) => ({
    _id: pkgName,
    _rev: 'hash',
    users: (
      opts.method === 'PUT'
        ? { foo: true }
        : {}
    ),
  })
  npmlog.info = (title, msg, id) => {
    t.equal(title, 'star', 'should use expected title')
    t.equal(msg, 'starring', 'should use expected msg')
    t.equal(id, pkgName, 'should use expected id')
  }
  star([pkgName], err => {
    if (err)
      throw err
    t.equal(
      result,
      '(*) @npmcli/arborist',
      'should output starred package msg'
    )
  })
})

t.test('unstar a package', t => {
  t.plan(4)
  const pkgName = '@npmcli/arborist'
  npm.config.get = key => key === 'star.unstar'
  npmFetch.json = async (uri, opts) => ({
    _id: pkgName,
    _rev: 'hash',
    ...(opts.method === 'PUT'
      ? {}
      : { foo: true }
    ),
  })
  npmlog.info = (title, msg, id) => {
    t.equal(title, 'unstar', 'should use expected title')
    t.equal(msg, 'unstarring', 'should use expected msg')
    t.equal(id, pkgName, 'should use expected id')
  }
  star([pkgName], err => {
    if (err)
      throw err
    t.equal(
      result,
      '( ) @npmcli/arborist',
      'should output unstarred package msg'
    )
  })
})

t.test('unicode', async t => {
  t.test('star a package', t => {
    npm.flatOptions.unicode = true
    npmFetch.json = async (uri, opts) => ({})
    star(['pkg'], err => {
      if (err)
        throw err
      t.equal(
        result,
        '\u2605  pkg',
        'should output unicode starred package msg'
      )
      t.end()
    })
  })

  t.test('unstar a package', t => {
    npm.flatOptions.unicode = true
    npm.config.get = key => key === 'star.unstar'
    npmFetch.json = async (uri, opts) => ({})
    star(['pkg'], err => {
      if (err)
        throw err
      t.equal(
        result,
        '\u2606  pkg',
        'should output unstarred package msg'
      )
      t.end()
    })
  })
})

t.test('logged out user', t => {
  const star = requireInject('../../lib/star.js', {
    ...mocks,
    '../../lib/utils/get-identity.js': async () => undefined,
  })
  star(['@npmcli/arborist'], err => {
    t.match(
      err,
      /You need to be logged in/,
      'should throw login required error'
    )
    t.end()
  })
})
