const t = require('tap')

let result = ''

const noop = () => null
const npm = {
  config: { get () {} },
  flatOptions: {},
  output: (...msg) => {
    result = [result, ...msg].join('\n')
  },
}
const npmFetch = { json: noop }
const log = { warn: noop }
const mocks = {
  'proc-log': log,
  'npm-registry-fetch': npmFetch,
  '../../../lib/utils/get-identity.js': async () => 'foo',
}

const Stars = t.mock('../../../lib/commands/stars.js', mocks)
const stars = new Stars(npm)

t.afterEach(() => {
  npm.config = { get () {} }
  log.warn = noop
  result = ''
})

t.test('no args', async t => {
  npmFetch.json = async (uri, opts) => {
    t.equal(uri, '/-/_view/starredByUser', 'should fetch from expected uri')
    t.equal(opts.query.key, '"foo"', 'should match logged in username')

    return {
      rows: [
        { value: '@npmcli/arborist' },
        { value: '@npmcli/map-workspaces' },
        { value: 'libnpmfund' },
        { value: 'libnpmpublish' },
        { value: 'ipt' },
      ],
    }
  }

  await stars.exec([])

  t.matchSnapshot(
    result,
    'should output a list of starred packages'
  )
})

t.test('npm star <user>', async t => {
  t.plan(3)
  npmFetch.json = async (uri, opts) => {
    t.equal(uri, '/-/_view/starredByUser', 'should fetch from expected uri')
    t.equal(opts.query.key, '"ruyadorno"', 'should match username')

    return {
      rows: [{ value: '@npmcli/arborist' }],
    }
  }

  await stars.exec(['ruyadorno'])

  t.match(
    result,
    '@npmcli/arborist',
    'should output expected list of starred packages'
  )
})

t.test('unauthorized request', async t => {
  t.plan(4)
  npmFetch.json = async () => {
    throw Object.assign(
      new Error('Not logged in'),
      { code: 'ENEEDAUTH' }
    )
  }

  log.warn = (title, msg) => {
    t.equal(title, 'stars', 'should use expected title')
    t.equal(
      msg,
      'auth is required to look up your username',
      'should warn auth required msg'
    )
  }

  await t.rejects(
    stars.exec([]),
    /Not logged in/,
    'should throw unauthorized request msg'
  )

  t.equal(
    result,
    '',
    'should have empty output'
  )
})

t.test('unexpected error', async t => {
  npmFetch.json = async () => {
    throw new Error('ERROR')
  }

  log.warn = (title, msg) => {
    throw new Error('Should not output extra warning msgs')
  }

  await t.rejects(
    stars.exec([]),
    /ERROR/,
    'should throw unexpected error message'
  )
})

t.test('no pkg starred', async t => {
  t.plan(2)
  npmFetch.json = async (uri, opts) => ({ rows: [] })

  log.warn = (title, msg) => {
    t.equal(title, 'stars', 'should use expected title')
    t.equal(
      msg,
      'user has not starred any packages',
      'should warn no starred packages msg'
    )
  }

  await stars.exec([])
})
