const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('prefix', async t => {
  const { joinedOutput, npm } = await loadMockNpm(t)
  await npm.exec('prefix', [])
  t.equal(
    joinedOutput(),
    npm.prefix,
    'outputs npm.prefix'
  )
})
