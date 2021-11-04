const t = require('tap')
const { real: mockNpm } = require('../../fixtures/mock-npm')

const username = 'foo'
const { joinedOutput, Npm } = mockNpm(t, {
  '../../lib/utils/get-identity.js': () => Promise.resolve(username),
})
const npm = new Npm()

t.before(async () => {
  await npm.load()
})

t.test('npm whoami', async (t) => {
  await npm.exec('whoami', [])
  t.equal(joinedOutput(), username, 'should print username')
})

t.test('npm whoami --json', async (t) => {
  t.teardown(() => {
    npm.config.set('json', false)
  })
  npm.config.set('json', true)
  await npm.exec('whoami', [])
  t.equal(JSON.parse(joinedOutput()), username, 'should print username')
})
