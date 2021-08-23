const t = require('tap')
const { real: mockNpm } = require('../fixtures/mock-npm')

t.test('prefix', async (t) => {
  const { joinedOutput, command, npm } = mockNpm(t)
  await npm.load()
  await command('prefix')
  t.equal(
    joinedOutput(),
    npm.prefix,
    'outputs npm.prefix'
  )
})
