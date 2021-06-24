const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

let result = ''

const noop = () => null
const config = {
  unicode: false,
  'star.unstar': false,
}
const npm = mockNpm({
  config,
  output: (...msg) => {
    result += msg.join('\n')
  },
})
const npmFetch = { json: noop }
const npmlog = { error: noop, info: noop, verbose: noop }
const mocks = {
  npmlog,
  'npm-registry-fetch': npmFetch,
  '../../lib/utils/get-identity.js': async () => 'foo',
  '../../lib/utils/usage.js': () => 'usage instructions',
}

const Star = t.mock('../../lib/star.js', mocks)
const star = new Star(npm)

t.afterEach(() => {
  config.unicode = false
  config['star.unstar'] = false
  npmlog.info = noop
  result = ''
})

t.test('no args', t => {
  star.exec([], err => {
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
  star.exec([pkgName], err => {
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
  config['star.unstar'] = true
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
  star.exec([pkgName], err => {
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
    config.unicode = true
    npmFetch.json = async (uri, opts) => ({})
    star.exec(['pkg'], err => {
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
    config.unicode = true
    config['star.unstar'] = true
    npmFetch.json = async (uri, opts) => ({})
    star.exec(['pkg'], err => {
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
  const Star = t.mock('../../lib/star.js', {
    ...mocks,
    '../../lib/utils/get-identity.js': async () => undefined,
  })
  const star = new Star(npm)
  star.exec(['@npmcli/arborist'], err => {
    t.match(
      err,
      /You need to be logged in/,
      'should throw login required error'
    )
    t.end()
  })
})
