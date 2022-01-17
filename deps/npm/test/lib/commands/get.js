const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('should retrieve values from config', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t)
  const name = 'editor'
  const value = 'vigor'
  npm.config.set(name, value)
  await npm.exec('get', [name])
  t.equal(
    joinedOutput(),
    value,
    'outputs config item'
  )
})
