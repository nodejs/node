const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('prefix', async (t) => {
  const { joinedOutput, npm } = await loadMockNpm(t)
  await npm.exec('root', [])
  t.equal(
    joinedOutput(),
    npm.dir,
    'outputs npm.dir'
  )
})
