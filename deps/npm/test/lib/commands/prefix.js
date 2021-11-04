const t = require('tap')
const { real: mockNpm } = require('../../fixtures/mock-npm')

t.test('prefix', async t => {
  const { joinedOutput, Npm } = mockNpm(t)
  const npm = new Npm()
  await npm.exec('prefix', [])
  t.equal(
    joinedOutput(),
    npm.prefix,
    'outputs npm.prefix'
  )
})
