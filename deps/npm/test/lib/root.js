const t = require('tap')
const { real: mockNpm } = require('../fixtures/mock-npm')

t.test('prefix', async (t) => {
  const { joinedOutput, command, npm } = mockNpm(t)
  await npm.load()
  await command('root')
  t.equal(
    joinedOutput(),
    npm.dir,
    'outputs npm.dir'
  )
})
