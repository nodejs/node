const t = require('tap')
const { real: mockNpm } = require('../fixtures/mock-npm')

const username = 'foo'
const { joinedOutput, command, npm } = mockNpm(t, {
  '../../lib/utils/get-identity.js': () => Promise.resolve(username),
})

t.before(async () => {
  await npm.load()
})

t.test('npm whoami', async (t) => {
  await command('whoami')
  t.equal(joinedOutput(), username, 'should print username')
})

t.test('npm whoami --json', async (t) => {
  t.teardown(() => {
    npm.config.set('json', false)
  })
  npm.config.set('json', true)
  await command('whoami')
  t.equal(JSON.parse(joinedOutput()), username, 'should print username')
})
