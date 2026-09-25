const t = require('tap')
const fs = require('node:fs')
const path = require('node:path')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')
const mockGlobals = require('@npmcli/mock-globals')
const libpack = require('libnpmpack')

const token = 'test-auth-token'
const authConfig = { '//registry.npmjs.org/:_authToken': token }
const stageId = '1de6f3db-2ed9-4d72-b3dd-8f0e2b474a2f'

t.test('downloads a staged tarball', async t => {
  const { npm, joinedOutput, prefix } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npmcli/test-package',
        version: '1.0.0',
      }),
      'index.js': 'module.exports = 42',
    },
  })
  const tarballData = await libpack(prefix)
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get(`/-/stage/${stageId}/tarball`)
    .reply(200, tarballData, { 'content-type': 'application/octet-stream' })

  mockGlobals(t, { 'process.cwd': () => prefix })

  await npm.exec('stage', ['download', stageId])
  const out = joinedOutput()
  const expectedFilename = `npmcli-test-package-1.0.0-${stageId}.tgz`
  t.match(out, expectedFilename)
  t.ok(fs.existsSync(path.join(prefix, expectedFilename)))
})

t.test('downloads with --json', async t => {
  const { npm, joinedOutput, prefix } = await loadMockNpm(t, {
    config: { ...authConfig, json: true },
    prefixDir: {
      'package.json': JSON.stringify({
        name: '@npmcli/test-package',
        version: '1.0.0',
      }),
    },
  })
  const tarballData = await libpack(prefix)
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get(`/-/stage/${stageId}/tarball`)
    .reply(200, tarballData, { 'content-type': 'application/octet-stream' })

  mockGlobals(t, { 'process.cwd': () => prefix })

  await npm.exec('stage', ['download', stageId])
  const out = joinedOutput()
  t.notMatch(out, `npmcli-test-package-1.0.0-${stageId}.tgz`)
  const expectedFilename = `npmcli-test-package-1.0.0-${stageId}.tgz`
  t.ok(fs.existsSync(path.join(prefix, expectedFilename)))
})

t.test('throws usageError without stage-id', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  await t.rejects(npm.exec('stage', ['download']), {
    code: 'EUSAGE',
  })
})

t.test('throws on invalid uuid', async t => {
  const { npm } = await loadMockNpm(t, {
    config: authConfig,
  })
  await t.rejects(npm.exec('stage', ['download', 'not-a-uuid']), {
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
  registry.nock.get(`/-/stage/${stageId}/tarball`)
    .reply(404, { error: 'Not found' })
  await t.rejects(npm.exec('stage', ['download', stageId]), {
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
  registry.nock.get(`/-/stage/${stageId}/tarball`)
    .reply(403, { error: 'You do not have permission to download this package' })
  await t.rejects(npm.exec('stage', ['download', stageId]), {
    statusCode: 403,
  })
})

t.test('throws when tarball has no package.json', async t => {
  const { npm, prefix } = await loadMockNpm(t, {
    config: authConfig,
    prefixDir: {},
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  // Empty tar (two 512-byte zero blocks) has no entries
  registry.nock.get(`/-/stage/${stageId}/tarball`)
    .reply(200, Buffer.alloc(1024), { 'content-type': 'application/octet-stream' })

  mockGlobals(t, { 'process.cwd': () => prefix })

  await t.rejects(npm.exec('stage', ['download', stageId]), {
    message: /Could not read package.json from tarball/,
  })
})
