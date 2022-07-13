const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')
const MockRegistry = require('../../fixtures/mock-registry.js')

const pkgName = '@npmcli/test-package'
const authToken = 'test-auth-token'
const username = 'test-user'
const auth = { '//registry.npmjs.org/:_authToken': authToken }

t.test('no args', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('unstar', []),
    { code: 'EUSAGE' },
    'should throw usage error'
  )
})

t.test('unstar a package unicode:false', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { unicode: false, ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  const manifest = registry.manifest({ name: pkgName, users: { [username]: true } })
  await registry.package({ manifest, query: { write: true } })
  registry.whoami({ username })
  registry.star(manifest, {})

  await npm.exec('unstar', [pkgName])
  t.equal(
    joinedOutput(),
    '( ) @npmcli/test-package',
    'should output unstarred package msg'
  )
})

t.test('unstar a package unicode:true', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { unicode: true, ...auth },
  })

  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  const manifest = registry.manifest({ name: pkgName, users: { [username]: true } })
  await registry.package({ manifest, query: { write: true } })
  registry.whoami({ username })
  registry.star(manifest, {})

  await npm.exec('unstar', [pkgName])
  t.equal(
    joinedOutput(),
    '☆  @npmcli/test-package',
    'should output unstarred package msg'
  )
})
