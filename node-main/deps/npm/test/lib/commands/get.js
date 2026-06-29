const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('should retrieve values from config', async t => {
  const name = 'editor'
  const value = 'vigor'
  const { joinedOutput, npm } = await loadMockNpm(t, {
    config: { [name]: value },
  })
  await npm.exec('get', [name])
  t.equal(
    joinedOutput(),
    value,
    'outputs config item'
  )
})
