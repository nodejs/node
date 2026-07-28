const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

const token = 'test-auth-token'
const authConfig = { '//registry.npmjs.org/:_authToken': token }
const stageId = '1de6f3db-2ed9-4d72-b3dd-8f0e2b474a2f'

t.test('rejects a staged package', async t => {
  const { npm, joinedOutput, logs } = await loadMockNpm(t, {
    config: { ...authConfig, otp: '123456' },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.delete(`/-/stage/${stageId}`).reply(204)
  await npm.exec('stage', ['reject', stageId])
  t.match(joinedOutput(), /has been rejected/)
  t.match(logs.warn, [/permanently delete/])
})

t.test('throws usageError without stage-id', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  await t.rejects(npm.exec('stage', ['reject']), {
    code: 'EUSAGE',
  })
})

t.test('throws on invalid uuid', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  await t.rejects(npm.exec('stage', ['reject', 'not-a-uuid']), {
    message: /stage-id must be a valid UUID/,
  })
})

t.test('throws on 404 (stage-id not found)', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { ...authConfig, otp: '123456' },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.delete(`/-/stage/${stageId}`)
    .reply(404, { error: 'Not found' })
  await t.rejects(npm.exec('stage', ['reject', stageId]), {
    statusCode: 404,
  })
})

t.test('throws on 403 (not an owner)', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { ...authConfig, otp: '123456' },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.delete(`/-/stage/${stageId}`)
    .reply(403, { error: 'You do not have permission to reject this package' })
  await t.rejects(npm.exec('stage', ['reject', stageId]), {
    statusCode: 403,
  })
})
