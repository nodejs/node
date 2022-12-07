const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const MockRegistry = require('@npmcli/mock-registry')
const nock = require('nock')

const username = 'foo'
const auth = { '//registry.npmjs.org/:_authToken': 'test-auth-token' }

t.test('npm whoami', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, { config: auth })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.whoami({ username })
  await npm.exec('whoami', [])
  t.equal(joinedOutput(), username, 'should print username')
})

t.test('npm whoami --json', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      json: true,
      ...auth,
    },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: 'test-auth-token',
  })
  registry.whoami({ username })
  await npm.exec('whoami', [])
  t.equal(JSON.parse(joinedOutput()), username, 'should print username')
})

t.test('npm whoami using mTLS', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, { config: {
    '//registry.npmjs.org/:certfile': '/some.cert',
    '//registry.npmjs.org/:keyfile': '/some.key',
  } })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
  })
  registry.whoami({ username })
  await npm.exec('whoami', [])
  t.equal(joinedOutput(), username, 'should print username')
})

t.test('credentials from token', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:username': username,
      '//registry.npmjs.org/:_password': 'hunter2',
    },
  })
  await npm.exec('whoami', [])
  t.equal(joinedOutput(), username, 'should print username')
})

t.test('not logged in', async t => {
  const { npm } = await loadMockNpm(t, {
    config: {
      json: true,
    },
  })
  await t.rejects(npm.exec('whoami', []), { code: 'ENEEDAUTH' })
})

t.test('non-string username in response', async t => {
  nock.disableNetConnect()
  t.teardown(() => {
    nock.enableNetConnect()
  })

  const server = nock('https://registry.npmjs.org', {
    reqheaders: {
      authorization: 'Bearer abcd1234',
    },
  })
    .get('/-/whoami')
    .reply(200, { username: null })

  const { npm } = await loadMockNpm(t, {
    config: {
      '//registry.npmjs.org/:_authToken': 'abcd1234',
    },
  })

  await t.rejects(npm.exec('whoami', []), { code: 'ENEEDAUTH' })
  t.ok(server.isDone())
})
