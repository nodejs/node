const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

const token = 'test-auth-token'
const authConfig = { '//registry.npmjs.org/:_authToken': token }

const stageItem = {
  id: '1de6f3db-2ed9-4d72-b3dd-8f0e2b474a2f',
  packageName: '@npmcli/example-package',
  version: '1.2.3',
  tag: 'latest',
  createdAt: '2026-03-16T09:00:00.000Z',
  actor: 'octocat',
  actorType: 'user',
  shasum: '4f7f5f1d5bcf2f72f6e4d6c4f3b2812d8a2f6c19',
}

t.test('views a staged package', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: authConfig,
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get(`/-/stage/${stageItem.id}`).reply(200, stageItem)
  await npm.exec('stage', ['view', stageItem.id])
  const out = joinedOutput()
  t.match(out, /id:/)
  t.match(out, 'package name: @npmcli/example-package')
  t.match(out, 'version: 1.2.3')
})

t.test('views with --json', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig, json: true },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get(`/-/stage/${stageItem.id}`).reply(200, stageItem)
  await npm.exec('stage', ['view', stageItem.id])
  const out = JSON.parse(joinedOutput())
  t.ok(out.id)
  t.equal(out.packageName, '@npmcli/example-package')
})

t.test('throws usageError without stage-id', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  await t.rejects(npm.exec('stage', ['view']), {
    code: 'EUSAGE',
  })
})

t.test('throws on invalid uuid', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  await t.rejects(npm.exec('stage', ['view', 'not-a-uuid']), {
    message: /stage-id must be a valid UUID/,
  })
})

t.test('throws on 404 (stage-id not found)', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get(`/-/stage/${stageItem.id}`)
    .reply(404, { error: 'Not found' })
  await t.rejects(npm.exec('stage', ['view', stageItem.id]), {
    statusCode: 404,
  })
})

t.test('throws on 403 (not an owner)', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get(`/-/stage/${stageItem.id}`)
    .reply(403, { error: 'You do not have permission to view this package' })
  await t.rejects(npm.exec('stage', ['view', stageItem.id]), {
    statusCode: 403,
  })
})
