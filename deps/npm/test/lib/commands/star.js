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
    npm.exec('star', []),
    { code: 'EUSAGE' },
    'should throw usage error'
  )
})

t.test('first person to star a package unicode:false', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { unicode: false, ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  const manifest = registry.manifest({ name: pkgName })
  await registry.package({ manifest, query: { write: true } })
  registry.whoami({ username })
  registry.star(manifest, { [username]: true })

  await npm.exec('star', [pkgName])
  t.equal(
    joinedOutput(),
    '(*) @npmcli/test-package',
    'should output starred package msg'
  )
})

t.test('second person to star a package unicode:true', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { unicode: true, ...auth },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: authToken,
  })
  const manifest = registry.manifest({ name: pkgName, users: { otheruser: true } })
  await registry.package({ manifest, query: { write: true } })
  registry.whoami({ username })
  registry.star(manifest, { otheruser: true, [username]: true })

  await npm.exec('star', [pkgName])
  t.equal(
    joinedOutput(),
    '★  @npmcli/test-package',
    'should output starred package msg'
  )
})
