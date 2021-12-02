const t = require('tap')
const { load: _loadMockNpm } = require('../../fixtures/mock-npm')

const username = 'foo'
const loadMockNpm = (t, options) => _loadMockNpm(t, {
  mocks: {
    '../../lib/utils/get-identity.js': () => Promise.resolve(username),
  },
  ...options,
})

t.test('npm whoami', async (t) => {
  const { npm, joinedOutput } = await loadMockNpm(t)
  await npm.exec('whoami', [])
  t.equal(joinedOutput(), username, 'should print username')
})

t.test('npm whoami --json', async (t) => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { json: true },
  })
  await npm.exec('whoami', [])
  t.equal(JSON.parse(joinedOutput()), username, 'should print username')
})
