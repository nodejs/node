const t = require('tap')
const { load: loadMockNpm } = require('../../../fixtures/mock-npm.js')
const MockRegistry = require('@npmcli/mock-registry')

const token = 'test-auth-token'
const authConfig = { '//registry.npmjs.org/:_authToken': token }

const stageItems = [
  {
    id: '1de6f3db-2ed9-4d72-b3dd-8f0e2b474a2f',
    packageName: '@npmcli/example-package',
    version: '1.2.3',
    tag: 'latest',
    createdAt: '2026-03-16T09:00:00.000Z',
    actor: 'octocat',
    actorType: 'user',
    shasum: '4f7f5f1d5bcf2f72f6e4d6c4f3b2812d8a2f6c19',
  },
  {
    id: 'f8e7a45b-7a5f-4f31-8e6d-9dd1c6ef38c0',
    packageName: 'example-lib',
    version: '0.4.0',
    tag: 'next',
    createdAt: '2026-03-15T18:22:11.000Z',
    actor: 'npm-bot',
    actorType: 'trusted automation',
    shasum: '8eb3b4e9b6e3d0d2c86be1e6d4f43f4be62e80ad',
  },
]

t.test('lists all staged packages', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get('/-/stage?page=0&perPage=100')
    .reply(200, { items: stageItems, page: 0, perPage: 100, total: 2 })
  await npm.exec('stage', ['list'])
  const out = joinedOutput()
  t.match(out, 'package name: @npmcli/example-package')
  t.match(out, 'package name: example-lib')
  t.match(out, 'version: 1.2.3')
  t.match(out, 'version: 0.4.0')
})

t.test('lists with package filter', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get('/-/stage?page=0&perPage=100&package=%40npmcli%2Fexample-package')
    .reply(200, { items: [stageItems[0]], page: 0, perPage: 100, total: 1 })
  await npm.exec('stage', ['list', '@npmcli/example-package'])
  const out = joinedOutput()
  t.match(out, '@npmcli/example-package')
  t.notMatch(out, 'example-lib')
})

t.test('lists with --json', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig, json: true },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get('/-/stage?page=0&perPage=100')
    .reply(200, { items: stageItems, page: 0, perPage: 100, total: 2 })
  await npm.exec('stage', ['list'])
  const out = JSON.parse(joinedOutput())
  t.equal(out.length, 2)
  t.equal(out[0].packageName, '@npmcli/example-package')
  t.equal(out[0].id, '1de6f3db-2ed9-4d72-b3dd-8f0e2b474a2f', 'uuid id is not redacted')
})

t.test('shows message when no packages', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get('/-/stage?page=0&perPage=100')
    .reply(200, { items: [], page: 0, perPage: 100, total: 0 })
  await npm.exec('stage', ['list'])
  t.match(joinedOutput(), 'No staged packages found.')
})

t.test('shows filtered message when no packages with filter', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  registry.nock.get('/-/stage?page=0&perPage=100&package=nonexistent')
    .reply(200, { items: [], page: 0, perPage: 100, total: 0 })
  await npm.exec('stage', ['list', 'nonexistent'])
  t.match(joinedOutput(), 'No staged versions of package name "nonexistent".')
})

t.test('throws on version specifier', async t => {
  const { npm } = await loadMockNpm(t, {
    config: { ...authConfig },
  })
  await t.rejects(npm.exec('stage', ['list', 'area@1.0.0']), {
    code: 'EUSAGE',
  })
})

t.test('paginates through multiple pages', async t => {
  const { npm, joinedOutput } = await loadMockNpm(t, {
    config: { ...authConfig },
  })
  const registry = new MockRegistry({
    tap: t,
    registry: npm.config.get('registry'),
    authorization: token,
  })
  // page 0: 100 items, total 101
  const page0Items = Array.from({ length: 100 }, (_, i) => ({
    ...stageItems[0],
    id: `id-${i}`,
    version: `1.0.${i}`,
  }))
  registry.nock.get('/-/stage?page=0&perPage=100')
    .reply(200, { items: page0Items, page: 0, perPage: 100, total: 101 })
  registry.nock.get('/-/stage?page=1&perPage=100')
    .reply(200, { items: [stageItems[1]], page: 1, perPage: 100, total: 101 })
  await npm.exec('stage', ['list'])
  const out = joinedOutput()
  t.match(out, 'version: 1.0.0')
  t.match(out, 'version: 0.4.0')
})
