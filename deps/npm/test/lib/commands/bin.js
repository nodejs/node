const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')
const mockGlobals = require('../../fixtures/mock-globals')

t.test('bin not global', async t => {
  const { npm, joinedOutput, logs } = await loadMockNpm(t)
  await npm.exec('bin', [])
  t.match(joinedOutput(), npm.localBin)
  t.match(logs.error, [])
})

t.test('bin global in env.path', async t => {
  const { npm, joinedOutput, logs } = await loadMockNpm(t, {
    config: { global: true },
  })

  mockGlobals(t, {
    'process.env': { path: npm.globalBin },
  })
  await npm.exec('bin', [])
  t.match(joinedOutput(), npm.globalBin)
  t.match(logs.error, [])
})

t.test('bin not in path', async t => {
  const { npm, joinedOutput, logs } = await loadMockNpm(t, {
    config: { global: true },
  })
  await npm.exec('bin', [])
  t.match(joinedOutput(), npm.globalBin)
  t.match(logs.error, [['bin', '(not in PATH env variable)']])
})
